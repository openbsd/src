/*	$OpenBSD: crimebus.h,v 1.4 2004/08/11 09:14:07 xsa Exp $	*/

/*
 * Copyright (c) 2003-2004 Opsycon AB (www.opsycon.se).
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_CRIMEBUS_H_
#define	_CRIMEBUS_H_

#include <machine/bus.h>

#define	CRIMEBUS_BASE		0x14000000

#define	CRIME_REVISION		 0x0000

#define	CRIME_CONTROL		 0x0008
#define	CRIME_CTRL_TRITON_SYSADC   0x2000
#define	CRIME_CTRL_CRIME_SYSADC    0x1000
#define	CRIME_CTRL_HARD_RESET      0x0800
#define	CRIME_CTRL_SOFT_RESET      0x0400
#define	CRIME_CTRL_DOG_ENABLE      0x0200
#define	CRIME_CTRL_ENDIAN_BIG      0x0100

#define	CRIME_INT_STAT		 0x0010
#define	CRIME_INT_MASK		 0x0018
#define	CRIME_INT_SOFT		 0x0020
#define	CRIME_INT_HARD		 0x0028

/*
 * CRIME_INT_STAT and CRIME_INT_MASK mapping.
 */
#define	CRIME_INT_VIDEO_IN_1	0x00000001	/* Video in 1 */
#define	CRIME_INT_VIDEO_IN_2	0x00000002	/* Video in 2 */
#define	CRIME_INT_VIDEO_OUT	0x00000004	/* Video out */
#define	CRIME_INT_MACE_ETHER	0x00000008	/* Mace ethernet NIC */
#define	CRIME_INT_SUPER_IO	0x00000010	/* Super I/O sub interrupt */
#define	CRIME_INT_SUB_MISC	0x00000020	/* Misc ??? */
#define	CRIME_INT_SUB_AUDIO	0x00000040	/* Audio sub interrupt */
#define	CRIME_INT_PCI_BRIDGE	0x00000080	/* PCI bridge errors */
#define	CRIME_INT_PCI_SCSI_0	0x00000100	/* AIC SCSI controller 0 */
#define	CRIME_INT_PCI_SCSI_1	0x00000200	/* AIC SCSI controller 1 */
#define	CRIME_INT_PCI_SLOT_0	0x00000400	/* PCI expansion slot 0 */
#define	CRIME_INT_PCI_SLOT_1	0x00000800	/* PCI expansion slot 1 */
#define	CRIME_INT_PCI_SLOT_2	0x00001000	/* PCI expansion slot 2 */
#define	CRIME_INT_PCI_SHARE_0	0x00002000	/* PCI shared 0 */
#define	CRIME_INT_PCI_SHARE_1	0x00004000	/* PCI shared 1 */
#define	CRIME_INT_PCI_SHARE_2	0x00008000	/* PCI shared 2 */
#define	CRIME_INT_GBE_0		0x00010000	/* GBE0 (E) */
#define	CRIME_INT_GBE_1		0x00020000	/* GBE1 (E) */
#define	CRIME_INT_GBE_2		0x00040000	/* GBE2 (E) */
#define	CRIME_INT_GBE_3		0x00080000	/* GBE3 (E) */
#define	CRIME_INT_CPU_ERR	0x00100000	/* CPU Errors */
#define	CRIME_INT_MEM_ERR	0x00200000	/* MEMORY Errors */
#define	CRIME_INT_RE_EDGE_EMPTY	0x00400000	/* RE */
#define	CRIME_INT_RE_EDGE_FULL	0x00800000	/* RE */
#define	CRIME_INT_RE_EDGE_IDLE	0x01000000	/* RE */
#define	CRIME_INT_RE_LEVL_EMPTY	0x02000000	/* RE */
#define	CRIME_INT_RE_LEVL_FULL	0x04000000	/* RE */
#define	CRIME_INT_RE_LEVL_IDLE	0x08000000	/* RE */
#define	CRIME_INT_SOFT_0	0x10000000	/* ??? */
#define	CRIME_INT_SOFT_1	0x20000000	/* ??? */
#define	CRIME_INT_SOFT_2	0x40000000	/* ??? */
#define	CRIME_INT_VICE		0x80000000	/* ??? */


/*
 *  Watchdog?
 */
#define	CRIME_KICK_DOG		 0x0030
#define	CRIME_TIMER		 0x0038

#define	CRIME_CPU_ERROR_ADDR	 0x0040
#define	CRIME_CPU_ERROR_STAT	 0x0048
#define	CRIME_CPU_ERROR_ENAB	 0x0050

#define	CRIME_MEM_ERROR_STAT	 0x0250
#define	CRIME_MEM_ERROR_ADDR	 0x0258

extern bus_space_t crimebus_tag;

u_int8_t crime_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t crime_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t crime_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t crime_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void crime_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int8_t);
void crime_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int16_t);
void crime_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void crime_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int64_t);

int crime_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void crime_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int crime_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t, bus_size_t, bus_space_handle_t *);

#endif	/* _CRIMEBUS_H_ */
