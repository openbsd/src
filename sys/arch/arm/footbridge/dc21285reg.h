/*	$OpenBSD: dc21285reg.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: dc21285reg.h,v 1.3 2002/11/03 21:43:30 chris Exp $	*/

/*
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997,1998 Causality Limited
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * DC21285 register definitions
 */

/* PCI registers in CSR space */

#define VENDOR_ID		0x00
#define DC21285_VENDOR_ID	0x1011
#define DEVICE_ID		0x02
#define DC21285_DEVICE_ID	0x1065
#define REVISION		0x08
#define CLASS			0x0A

/* Other PCI control / status registers */

#define	OUTBOUND_INT_STATUS	0x030
#define	OUTBOUND_INT_MASK	0x034
#define	I2O_INBOUND_FIFO	0x040
#define I2O_OUTBOUND_FIFO	0x044

/* Mailbox registers */

#define	MAILBOX_0		0x050
#define	MAILBOX_1		0x054
#define	MAILBOX_2		0x058
#define	MAILBOX_3		0x05C

#define	DOORBELL		0x060
#define	DOORBELL_SETUP		0x064
#define	ROM_WRITE_BYTE_ADDRESS	0x068

/* DMA Channel registers */

#define	DMA_CHAN_1_BYTE_COUNT	0x80
#define	DMA_CHAN_1_PCI_ADDR	0x84
#define	DMA_CHAN_1_SDRAM_ADDR	0x88
#define	DMA_CHAN_1_DESCRIPT	0x8C
#define	DMA_CHAN_1_CONTROL	0x90
#define	DMA_CHAN_2_BYTE_COUNT	0xA0
#define	DMA_CHAN_2_PCI_ADDR	0xA4
#define	DMA_CHAN_2_SDRAM_ADDR	0xA8
#define	DMA_CHAN_2_DESCRIPTOR	0xAC
#define	DMA_CHAN_2_CONTROL	0xB0

/* Offsets into DMA descriptor */

#define	DMA_BYTE_COUNT		0
#define	DMA_PCI_ADDRESS		4
#define	DMA_SDRAM_ADDRESS	8
#define	DMA_NEXT_DESCRIPTOR	12

/* DMA byte count register bits */

#define	DMA_INTERBURST_SHIFT	24
#define	DMA_PCI_TO_SDRAM	0
#define	DMA_SDRAM_TO_PCI	(1 << 30)
#define	DMA_END_CHAIN		(1 << 31)

/* DMA control bits */

#define	DMA_ENABLE		(1 << 0)
#define	DMA_TRANSFER_DONE	(1 << 2)
#define	DMA_ERROR		(1 << 3)
#define	DMA_REGISTOR_DESCRIPTOR	(1 << 4)
#define	DMA_PCI_MEM_READ	(0 << 5)
#define	DMA_PCI_MEM_READ_LINE	(1 << 5)
#define	DMA_PCI_MEM_READ_MULTI1	(2 << 5)
#define	DMA_PCI_MEM_READ_MULTI2	(3 << 5)
#define	DMA_CHAIN_DONE		(1 << 7)
#define	DMA_INTERBURST_4	(0 << 8)
#define	DMA_INTERBURST_8	(1 << 8)
#define	DMA_INTERBURST_16	(2 << 8)
#define	DMA_INTERBURST_32	(3 << 8)
#define	DMA_PCI_LENGTH_8	0
#define	DMA_PCI_LENGTH_16	(1 << 15)
#define	DMA_SDRAM_LENGTH_1	(0 << 16)
#define	DMA_SDRAM_LENGTH_2	(1 << 16)
#define	DMA_SDRAM_LENGTH_4	(2 << 16)
#define	DMA_SDRAM_LENGTH_8	(3 << 16)
#define	DMA_SDRAM_LENGTH_16	(4 << 16)

/* CSR Base Address Mask */

#define	CSR_BA_MASK		0x0F8
#define	CSR_MASK_128B		0x00000000
#define	CSR_MASK_512KB		0x00040000
#define	CSR_MASK_1MB		0x000C0000
#define	CSR_MASK_2MB		0x001C0000
#define	CSR_MASK_4MB		0x003C0000
#define	CSR_MASK_8MB		0x007C0000
#define	CSR_MASK_16MB		0x00FC0000
#define	CSR_MASK_32MB		0x01FC0000
#define	CSR_MASK_64MB		0x03FC0000
#define	CSR_MASK_128MB		0x07FC0000
#define	CSR_MASK_256MB		0x0FFC0000
#define	CSR_BA_OFFSET		0x0FC

/* SDRAM Base Address Mask */

#define	SDRAM_BA_MASK		0x100
#define	SDRAM_MASK_256KB	0x00000000
#define	SDRAM_MASK_512KB	0x00040000
#define	SDRAM_MASK_1MB		0x000C0000
#define	SDRAM_MASK_2MB		0x001C0000
#define	SDRAM_MASK_4MB		0x003C0000
#define	SDRAM_MASK_8MB		0x007C0000
#define	SDRAM_MASK_16MB		0x00FC0000
#define	SDRAM_MASK_32MB		0x01FC0000
#define	SDRAM_MASK_64MB		0x03FC0000
#define	SDRAM_MASK_128MB	0x07FC0000
#define	SDRAM_MASK_256MB	0x0FFC0000
#define	SDRAM_WINDOW_DISABLE	(1 << 31)
#define	SDRAM_BA_OFFSET		0x104

/* Expansion ROM Base Address Mask */

#define EXPANSION_ROM_BA_MASK	0x108
#define	ROM_MASK_1MB		0x00000000
#define	ROM_MASK_2MB		0x00100000
#define	ROM_MASK_4MB		0x00300000
#define	ROM_MASK_8MB		0x00700000
#define	ROM_MASK_16MB		0x00F00000
#define	ROM_WINDOW_DISABLE	(1 << 31)

/* SDRAM configuration */

#define	SDRAM_TIMING		0x10C
#define SDRAM_ARRAY_SIZE_0	0x0
#define	SDRAM_ARRAY_SIZE_1MB	0x1
#define	SDRAM_ARRAY_SIZE_2MB	0x2
#define	SDRAM_ARRAY_SIZE_4MB	0x3
#define	SDRAM_ARRAY_SIZE_8MB	0x4
#define	SDRAM_ARRAY_SIZE_16MB	0x5
#define	SDRAM_ARRAY_SIZE_32MB	0x6
#define	SDRAM_ARRAY_SIZE_64MB	0x7
#define	SDRAM_2_BANKS		0
#define	SDRAM_4_BANKS		(1 << 3)
#define	SDRAM_ADDRESS_MUX_SHIFT	4
#define	SDRAM_ARRAY_BASE_SHIFT	20
#define	SDRAM_ADDRESS_SIZE_0	0x110
#define	SDRAM_ADDRESS_SIZE_1	0x114
#define	SDRAM_ADDRESS_SIZE_2	0x118
#define	SDRAM_ADDRESS_SIZE_3	0x11C

/* I2O registers */

#define	I2O_INBOUND_FREE_HEAD	0x120
#define	I2O_INBOUND_POST_TAIL	0x124
#define	I2O_OUTBOUND_POST_HEAD	0x128
#define I2O_OUTBOUND_FREE_TAIL	0x12c
#define	I2O_INBOUND_FREE_COUNT	0x130
#define	I2O_OUTBOUND_POST_COUNT	0x134
#define	I2O_INBOUND_POST_COUNT	0x138

/* Control register */

#define	SA_CONTROL		0x13C
#define	INITIALIZE_COMPLETE	(1 << 0)
#define	ASSERT_SERR		(1 << 1)
#define	RECEIVED_SERR		(1 << 3)
#define	SA_SDRAM_PARITY_ERROR	(1 << 4)
#define	PCI_SDRAM_PARITY_ERROR	(1 << 5)
#define	DMA_SDRAM_PARITY_ERROR	(1 << 6)
#define	DISCARD_TIMER_EXPIRED	(1 << 8)
#define	PCI_NOT_RESET		(1 << 9)
#define	WATCHDOG_ENABLE		(1 << 13)
#define	I2O_SIZE_256		(0 << 10)
#define	I2O_SIZE_512		(1 << 10)
#define	I2O_SIZE_1024		(2 << 10)
#define	I2O_SIZE_2048		(3 << 10)
#define	I2O_SIZE_4096		(4 << 10)
#define	I2O_SIZE_8192		(5 << 10)
#define	I2O_SIZE_16384		(6 << 10)
#define	I2O_SIZE_32768		(7 << 10)
#define	ROM_WIDTH_8		(3 << 14)
#define	ROM_WIDTH_16		(1 << 14)
#define	ROM_WIDTH_32		(2 << 14)
#define	ROM_ACCESS_TIME_SHIFT	16
#define	ROM_BURST_TIME_SHIFT	20
#define	ROM_TRISTATE_TIME_SHIFT	24
#define	XCS_DIRECTION_SHIFT	28
#define	PCI_CENTRAL_FUNCTION	(1 << 31)

#define	PCI_ADDRESS_EXTENSION	0x140
#define	PREFETCHABLE_MEM_RANGE	0x144

/* XBUS / PCI Arbiter registers */

#define	XBUS_CYCLE_ARBITER	0x148
#define	XBUS_CYCLE_0_SHIFT	0
#define	XBUS_CYCLE_1_SHIFT	3
#define	XBUS_CYCLE_2_SHIFT	6
#define	XBUS_CYCLE_3_SHIFT	9
#define	XBUS_CYCLE_STROBE_SHIFT	12
#define	XBUS_PCI_ARBITER	(1 << 23)
#define	XBUS_INT_IN_L0_LOW	0
#define	XBUS_INT_IN_L0_HIGH	(1 << 24)
#define	XBUS_INT_IN_L1_LOW	0
#define	XBUS_INT_IN_L1_HIGH	(1 << 25)
#define	XBUS_INT_IN_L2_LOW	0
#define	XBUS_INT_IN_L2_HIGH	(1 << 26)
#define	XBUS_INT_IN_L3_LOW	0
#define	XBUS_INT_IN_L3_HIGH	(1 << 27)
#define	XBUS_INT_XCS0_LOW	0
#define	XBUS_INT_XCS0_HIGH	(1 << 28)
#define	XBUS_INT_XCS1_LOW	0
#define	XBUS_INT_XCS1_HIGH	(1 << 29)
#define	XBUS_INT_XCS2_LOW	0
#define	XBUS_INT_XCS2_HIGH	(1 << 30)
#define	XBUS_PCI_INT_REQUEST	(1 << 31)

#define	XBUS_IO_STROBE_MASK	0x14C
#define	XBUS_IO_STROBE_0_SHIFT	0
#define	XBUS_IO_STROBE_2_SHIFT	8
#define	XBUS_IO_STROBE_3_SHIFT	16
#define	XBUS_IO_STROBE_4_SHIFT	24

#define	DOORBELL_PCI_MASK	0x150
#define	DOORBELL_SA_MASK	0x154

/* UART registers */

#define	UART_DATA		0x160
#define	UART_RX_STAT		0x164
#define	UART_PARITY_ERROR	0x01
#define	UART_FRAME_ERROR	0x02
#define	UART_OVERRUN_ERROR	0x04
#define	UART_RX_ERROR		(UART_PARITY_ERROR | UART_FRAME_ERROR \
				| UART_OVERRUN_ERROR)
#define	UART_H_UBRLCR		0x168
#define	UART_BREAK		0x01
#define	UART_PARITY_ENABLE	0x02
#define	UART_ODD_PARITY		0x00
#define	UART_EVEN_PARITY	0x04
#define	UART_STOP_BITS_1	0x00
#define	UART_STOP_BITS_2	0x08
#define	UART_ENABLE_FIFO	0x10
#define	UART_DATA_BITS_5	0x00
#define	UART_DATA_BITS_6	0x20
#define	UART_DATA_BITS_7	0x40
#define	UART_DATA_BITS_8	0x60
#define	UART_M_UBRLCR		0x16C
#define	UART_L_UBRLCR		0x170
#define UART_BRD(fclk, x)	(((fclk) / 4 / 16 / x) - 1)

#define	UART_CONTROL		0x174
#define	UART_ENABLE		0x01
#define	UART_SIR_ENABLE		0x02
#define	UART_IRDA_ENABLE	0x04
#define	UART_FLAGS		0x178
#define	UART_TX_BUSY		0x08
#define	UART_RX_FULL		0x10
#define	UART_TX_EMPTY		0x20

/* Interrupt numbers for IRQ and FIQ registers */

#define	IRQ_RESERVED0		0x00
#define	IRQ_SOFTINT		0x01
#define	IRQ_SERIAL_RX		0x02
#define	IRQ_SERIAL_TX		0x03
#define	IRQ_TIMER_1		0x04
#define	IRQ_TIMER_2		0x05
#define	IRQ_TIMER_3		0x06
#define	IRQ_TIMER_4		0x07
#define	IRQ_IN_L0		0x08
#define	IRQ_IN_L1		0x09
#define	IRQ_IN_L2		0x0A
#define	IRQ_IN_L3		0x0B
#define	IRQ_XCS_L0		0x0C
#define	IRQ_XCS_L1		0x0D
#define	IRQ_XCS_L2		0x0E
#define	IRQ_DOORBELL		0x0F
#define	IRQ_DMA_1		0x10
#define	IRQ_DMA_2		0x11
#define	IRQ_PCI			0x12
#define	IRQ_PMCSR		0x13
#define	IRQ_RESERVED1		0x14
#define	IRQ_RESERVED2		0x15
#define	IRQ_BIST		0x16
#define	IRQ_SERR		0x17
#define	IRQ_SDRAM_PARITY	0x18
#define	IRQ_I2O			0x19
#define	IRQ_RESERVED3		0x1A
#define	IRQ_DISCARD_TIMER	0x1B
#define	IRQ_DATA_PARITY		0x1C
#define	IRQ_MASTER_ABORT	0x1D
#define	IRQ_TARGET_ABORT	0x1E
#define	IRQ_PARITY		0x1F

/* IRQ and FIQ status / enable registers */

#define	IRQ_STATUS		0x180
#define	IRQ_RAW_STATUS		0x184
#define IRQ_ENABLE		0x188
#define	IRQ_ENABLE_SET		0x188
#define	IRQ_ENABLE_CLEAR	0x18c
#define	IRQ_SOFT		0x190

#define	FIQ_STATUS		0x280
#define	FIQ_RAW_STATUS		0x284
#define FIQ_ENABLE		0x288
#define	FIQ_ENABLE_SET		0x288
#define	FIQ_ENABLE_CLEAR	0x28c
#define	FIQ_SOFT		0x290

/* Timer registers */

/* Relative offsets and bases */

#define	TIMER_LOAD		0x00
#define	TIMER_VALUE		0x04
#define	TIMER_CONTROL		0x08
#define	TIMER_CLEAR		0x0C
#define	TIMER_1_BASE		0x300
#define	TIMER_2_BASE		0x320
#define	TIMER_3_BASE		0x340
#define	TIMER_4_BASE		0x360

/* Control register bits */

#define	TIMER_FCLK		0x00
#define	TIMER_FCLK_16		0x04
#define	TIMER_FCLK_256		0x08
#define	TIMER_EXTERNAL		0x0C
#define	TIMER_MODE_FREERUN	0x00
#define	TIMER_MODE_PERIODIC	0x40
#define	TIMER_ENABLE		0x80

/* Maximum timer value */

#define TIMER_MAX_VAL		0x00FFFFFF

/* Specific registers */

#define	TIMER_1_LOAD		0x300
#define	TIMER_1_VALUE		0x304
#define	TIMER_1_CONTROL		0x308
#define	TIMER_1_CLEAR		0x30C
#define	TIMER_2_LOAD		0x320
#define	TIMER_2_VALUE		0x324
#define	TIMER_2_CONTROL		0x328
#define	TIMER_2_CLEAR		0x32C
#define	TIMER_3_LOAD		0x340
#define	TIMER_3_VALUE		0x344
#define	TIMER_3_CONTROL		0x348
#define	TIMER_3_CLEAR		0x34C
#define	TIMER_4_LOAD		0x360
#define	TIMER_4_VALUE		0x364
#define	TIMER_4_CONTROL		0x368
#define	TIMER_4_CLEAR		0x36C

/* Miscellaneous definitions */

#ifndef FCLK
#define FCLK 50000000
#endif
