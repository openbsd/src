/*	$OpenBSD: mpc106reg.h,v 1.4 1999/11/08 23:49:00 rahnds Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD for RTMX Inc
 *	by Per Fogelstrom, Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#define MPC106_V_PCI_MEM_SPACE	0xc0000000	/* Viritual */
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
#define MPC106_P_PCI_IO_SPACE_MAP_B	0xf0000000

/* Where we map the config space */
#define MPC106_PCI_CONF_SPACE_MAP_B \
	(MPC106_V_ISA_IO_SPACE_MAP_B + 0x00800000)

/* offsets from base pointer */
#define	MPC106_REGOFFS(x)	((x) | 0x80000000)

/* Where PCI devices sees CPU memory. */
#define	MPC106_PCI_CPUMEM	0x80000000

#if 0
static __inline void
mpc_cfg_write_1(iot, ioh, reg, data)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int32_t reg;
	u_int8_t val;
{
	bus_space_write_4(iot, ioh, 0xcf8, MPC106_REGOFFS(reg));
	bus_space_write_1(iot, ioh, 0xcfc, val);

	u_int32_t addr;
	int device;
	int s;
	int handle; 
	int tag = 0;
	printf("mpc_cfg_write tag %x offset %x data %x\n", tag, offset, data);

	device = (tag >> 11) & 0x1f;
	addr = (0x800 << device) | (tag & 0x380) | MPC106_REGOFFS(reg);

	handle = ppc_open_pci_bridge();
	s = splhigh();

	OF_call_method("config-l!", handle, 1, 1,
		0x80000000 | addr, &data);
	splx(s);
	ppc_close_pci_bridge(handle);
}

static __inline void
mpc_cfg_write_2(iot, ioh, reg, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int32_t reg;
	u_int16_t val;
{
	bus_space_write_4(iot, ioh, 0xcf8, MPC106_REGOFFS(reg));
	bus_space_write_2(iot, ioh, 0xcfc, val);
}

static __inline void
mpc_cfg_write_4(iot, ioh, reg, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int32_t reg;
	u_int32_t val;
{

	bus_space_write_4(iot, ioh, 0xcf8, MPC106_REGOFFS(reg));
	bus_space_write_4(iot, ioh, 0xcfc, val);
}
#endif

static __inline u_int8_t
mpc_cfg_read_1(iot, ioh, reg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int32_t reg;
{
	u_int8_t _v_;

	bus_space_write_4(iot, ioh, 0xcf8, MPC106_REGOFFS(reg));
	_v_ = bus_space_read_1(iot, ioh, 0xcfc);
	return(_v_);
}

#if 0
static __inline u_int16_t
mpc_cfg_read_2(iot, ioh, reg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int32_t reg;
{
	u_int16_t _v_;

	bus_space_write_4(iot, ioh, 0xcf8, MPC106_REGOFFS(reg));
	_v_ = bus_space_read_2(iot, ioh, 0xcfc);
	return(_v_);
}

static __inline u_int32_t
mpc_cfg_read_4(iot, ioh, reg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int32_t reg;
{
	u_int32_t _v_;

	bus_space_write_4(iot, ioh, 0xcf8, MPC106_REGOFFS(reg));
	_v_ = bus_space_read_4(iot, ioh, 0xcfc);
	return(_v_);
}
#endif

#define MPC106_PCI_VENDOR		0x00
#define MPC106_PCI_DEVICE		0x02
#define MPC106_PCI_CMD			0x04
#define MPC106_PCI_STAT			0x06
#define MPC106_PCI_REVID		0x08

#define	MPC106_PCI_PMGMT		0x70

#endif /* _MACHINE_MPC106REG_H_ */
