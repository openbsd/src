/*	$OpenBSD: macebus.h,v 1.8 2007/10/18 18:59:29 jsing Exp $	*/

/*
 * Copyright (c) 2003-2004 Opsycon AB (www.opsycon.com).
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

#ifndef	_MACEBUS_H_
#define	_MACEBUS_H_ 1

#include <machine/bus.h>

/*
 *  Physical address of MACEBUS.
 */
#define	MACEBUS_BASE		0x1f000000

/*
 *  Offsets for various I/O sections on MACEBUS
 */
#define	MACE_PCI_OFFS		0x00080000
#define	MACE_VIN1_OFFS		0x00100000
#define	MACE_VIN2_OFFS		0x00180000
#define	MACE_VOUT_OFFS		0x00200000
#define	MACE_IO_OFFS		0x00300000
#define	MACE_ISAX_OFFS		0x00380000
#define	MACE_ISAX_SIZE		0x00020000

/*
 *  PCI control registers (relative MACE_PCI_OFFS)
 */
#define	MACE_PCI_ERROR_ADDRESS	0x0000
#define	MACE_PCI_ERROR_FLAGS	0x0004
#define	MACE_PCI_CONTROL	0x0008
#define	MACE_PCI_REVISION	0x000c
#define	MACE_PCI_FLUSH		0x000c
#define	MACE_PCI_CFGADDR	0x0cf8
#define	MACE_PCI_CFGDATA	0x0cfc

#define	MACE_PCI_INTCTRL	0x000000ff	/* Interrupt control mask */

/* PCI_ERROR_FLAGS Bits */
#define	PERR_MASTER_ABORT		0x80000000
#define	PERR_TARGET_ABORT		0x40000000
#define	PERR_DATA_PARITY_ERR		0x20000000
#define	PERR_RETRY_ERR			0x10000000
#define	PERR_ILLEGAL_CMD		0x08000000
#define	PERR_SYSTEM_ERR			0x04000000
#define	PERR_INTERRUPT_TEST		0x02000000
#define	PERR_PARITY_ERR			0x01000000
#define	PERR_OVERRUN			0x00800000
#define	PERR_RSVD			0x00400000
#define	PERR_MEMORY_ADDR		0x00200000
#define	PERR_CONFIG_ADDR		0x00100000
#define	PERR_MASTER_ABORT_ADDR_VALID	0x00080000
#define	PERR_TARGET_ABORT_ADDR_VALID	0x00040000
#define	PERR_DATA_PARITY_ADDR_VALID	0x00020000
#define	PERR_RETRY_ADDR_VALID		0x00010000


/*
 *  MACE ISA definitions.
 */
#define	MACE_ISA_OFFS		(MACE_IO_OFFS+0x00010000)

#define MACE_ISA_RING_BASE	0x0000
#define  MACE_ISA_RING_ALIGN	0x00010000
#define	MACE_ISA_MISC_REG	0x0008	/* Various status and controls */
#define	MACE_ISA_INT_STAT	0x0010
#define	MACE_ISA_INT_MASK	0x0018

/* MACE_ISA_MISC_REG definitions */
#define	MACE_ISA_MISC_RLED_OFF	0x0010	/* Turns off RED LED */
#define	MACE_ISA_MISC_GLED_OFF	0x0020	/* Turns off GREEN LED */

/* MACE_ISA_INT_* definitions */
#define	MACE_ISA_INT_AUDIO	0x000000ff	/* Audio ints */
#define  MACE_ISA_INT_AUDIO_SC	      0x02
#define  MACE_ISA_INT_AUDIO_DMA1      0x04
#define  MACE_ISA_INT_AUDIO_DMA2      0x10
#define  MACE_ISA_INT_AUDIO_DMA3      0x40      
#define	MACE_ISA_INT_RTC	0x00000100	/* RTC */
#define	MACE_ISA_INT_KBD	0x00000200	/* Keyboard */
#define	MACE_ISA_INT_KBD_POLL	0x00000400	/* Keyboard polled */
#define	MACE_ISA_INT_MOUSE	0x00000800	/* Mouse */
#define	MACE_ISA_INT_MOUSE_POLL	0x00001000	/* Mouse polled */
#define	MACE_ISA_INT_TIMER	0x0000e000	/* Timer/counter compare */
#define	MACE_ISA_INT_PARALLEL	0x000f0000	/* Parallel port */
#define	MACE_ISA_INT_SERIAL_1	0x03f00000	/* Serial port 1 */
#define	MACE_ISA_INT_SERIAL_2	0xfc000000	/* Serial port 2 */


/* ISA Peripherals */
#define	MACE_ISA_EPP_OFFS	(MACE_ISAX_OFFS+0x00000000)
#define	MACE_ISA_ECP_OFFS	(MACE_ISAX_OFFS+0x00008000)
#define	MACE_ISA_SER1_OFFS	(MACE_ISAX_OFFS+0x00010000)
#define	MACE_ISA_SER2_OFFS	(MACE_ISAX_OFFS+0x00018000)
#define	MACE_ISA_RTC_OFFS	(MACE_ISAX_OFFS+0x00020000)
#define	MACE_ISA_GAME_OFFS	(MACE_ISAX_OFFS+0x00030000)

extern bus_space_t macebus_tag;

u_int8_t mace_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t mace_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t mace_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t mace_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void mace_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int8_t);
void mace_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int16_t);
void mace_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void mace_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int64_t);

int mace_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void mace_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int mace_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t, bus_size_t, bus_space_handle_t *);

void *macebus_intr_establish(void *, u_long, int, int, int (*)(void *), void *, char *);

#endif	/* _MACEBUS_H_ */
