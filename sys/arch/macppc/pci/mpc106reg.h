/*	$OpenBSD: mpc106reg.h,v 1.3 2004/01/27 10:04:15 miod Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *
 * mpc106reg.h: PowerPC to PCI bridge controller
 *              This code will probably work with the 105 as well.
 */

#ifndef _MACHINE_MPC106REG_H_
#define _MACHINE_MPC106REG_H_

/* Where we map the PCI memory space - MAP A*/
#define MPC106_V_PCI_MEM_SPACE	0xc0000000	/* Virtual */
#define MPC106_P_PCI_MEM_SPACE	0xc0000000	/* Physical */

/* Where we map the PCI I/O space - MAP A*/
#define MPC106_P_ISA_IO_SPACE	0x80000000
#define MPC106_V_ISA_IO_SPACE	0x80000000
#define MPC106_V_PCI_IO_SPACE	0x80000000
#define MPC106_P_PCI_IO_SPACE	0x80000000

/* Where we map the config space */
#define MPC106_PCI_CONF_SPACE	(MPC106_V_ISA_IO_SPACE + 0x00800000)

/* Where we map the PCI memory space - MAP B*/
#define MPC106_P_PCI_MEM_SPACE_MAP_B	0x80000000	/* Physical */

/* Where we map the PCI I/O space - MAP B*/
#define MPC106_P_PCI_IO_SPACE_MAP_B	0xfe000000

/* offsets from base pointer */
#define	MPC106_REGOFFS(x)	((x) | 0x80000000)

/* Where PCI devices sees CPU memory. */
#define	MPC106_PCI_CPUMEM	0x80000000

#define MPC106_PCI_VENDOR		0x00
#define MPC106_PCI_DEVICE		0x02
#define MPC106_PCI_CMD			0x04
#define MPC106_PCI_STAT			0x06
#define MPC106_PCI_REVID		0x08

#define	MPC106_PCI_PMGMT		0x70

void
mpc_cfg_write_1( struct pcibr_config *cp, u_int32_t reg, u_int8_t val);
void
mpc_cfg_write_2( struct pcibr_config *cp, u_int32_t reg, u_int16_t val);
void
mpc_cfg_write_4( struct pcibr_config *cp, u_int32_t reg, u_int32_t val);

u_int8_t
mpc_cfg_read_1( struct pcibr_config *cp, u_int32_t reg);

u_int16_t
mpc_cfg_read_2( struct pcibr_config *cp, u_int32_t reg);

u_int32_t
mpc_cfg_read_4( struct pcibr_config *cp, u_int32_t reg);

#endif /* _MACHINE_MPC106REG_H_ */
