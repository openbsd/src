/*	$OpenBSD: mpc106reg.h,v 1.2 1998/08/25 07:40:47 pefo Exp $ */

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

/* Where we map the PCI memory space */
#define MPC106_V_PCI_MEM_SPACE	0xc0000000	/* Viritual */
#define MPC106_P_PCI_MEM_SPACE	0xc0000000	/* Physical */

/* Where we map the PCI I/O space */
#define MPC106_P_ISA_IO_SPACE	0x80000000
#define MPC106_V_ISA_IO_SPACE	0x80000000
#define MPC106_V_PCI_IO_SPACE	(MPC106_V_ISA_IO_SPACE + 0x01000000)

/* Where we map the config space */
#define MPC106_PCI_CONF_SPACE	(MPC106_V_ISA_IO_SPACE + 0x00800000)

/* offsets from base pointer */
#define MPC106_CONF_BASE	(MPC106_V_ISA_IO_SPACE + 0x0cf8)
#define MPC106_CONF_DATA	(MPC106_V_ISA_IO_SPACE + 0x0cfc)
#define	MPC106_REGOFFS(x)	((x << 24) | 0x80)

/* Where PCI devices sees CPU memory. */
#define	MPC106_PCI_CPUMEM	0x80000000

static __inline void
mpc_cfg_write_1(reg, val)
	u_int32_t reg;
	u_int8_t val;
{
	out32(MPC106_CONF_BASE, MPC106_REGOFFS(reg));
	outb(MPC106_CONF_DATA + (reg & 3), val);
}

static __inline void
mpc_cfg_write_2(reg, val)
	u_int32_t reg;
	u_int16_t val;
{
        u_int32_t _p_ = MPC106_CONF_DATA + (reg & 2);

	out32(MPC106_CONF_BASE, MPC106_REGOFFS(reg));
	__asm__ volatile("sthbrx %0, 0, %1\n" :: "r"(val), "r"(_p_));
	__asm__ volatile("sync"); __asm__ volatile("eieio");
}

static __inline void
mpc_cfg_write_4(reg, val)
	u_int32_t reg;
	u_int32_t val;
{
        u_int32_t _p_ = MPC106_CONF_DATA;

	out32(MPC106_CONF_BASE, MPC106_REGOFFS(reg));
	__asm__ volatile("stwbrx %0, 0, %1\n" :: "r"(val), "r"(_p_));
	__asm__ volatile("sync"); __asm__ volatile("eieio");
}

static __inline u_int8_t
mpc_cfg_read_1(reg)
	u_int32_t reg;
{
	u_int8_t _v_;

	out32(MPC106_CONF_BASE, MPC106_REGOFFS(reg));
	_v_ = inb(MPC106_CONF_DATA);
	return(_v_);
}

static __inline u_int16_t
mpc_cfg_read_2(reg)
	u_int32_t reg;
{
	u_int16_t _v_;
        u_int32_t _p_ = MPC106_CONF_DATA + (reg & 2);

	out32(MPC106_CONF_BASE, MPC106_REGOFFS(reg));
	__asm__ volatile("lhbrx %0, 0, %1\n" : "=r"(_v_) : "r"(_p_));
	__asm__ volatile("sync"); __asm__ volatile("eieio");
	return(_v_);
}

static __inline u_int32_t
mpc_cfg_read_4(reg)
	u_int32_t reg;
{
	u_int32_t _v_;
        u_int32_t _p_ = MPC106_CONF_DATA;

	out32(MPC106_CONF_BASE, MPC106_REGOFFS(reg));
	__asm__ volatile("lwbrx %0, 0, %1\n" : "=r"(_v_) : "r"(_p_));
	__asm__ volatile("sync"); __asm__ volatile("eieio");
	return(_v_);
}

#define MPC106_PCI_VENDOR		0x00
#define MPC106_PCI_DEVICE		0x02
#define MPC106_PCI_CMD			0x04
#define MPC106_PCI_STAT			0x06
#define MPC106_PCI_REVID		0x08

#define	MPC106_PCI_PMGMT		0x70

#endif /* _MACHINE_MPC106REG_H_ */
