/*	$OpenBSD: mc68681reg.h,v 1.1 2013/06/11 21:03:39 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * MC68HC681 Dual Asynchronous Receiver/Transmitter registers
 */

/*
 * Register Addresses
 */

#define	DART_MRA	0x00	/* Mode Register A (rw) */
#define	DART_SRA	0x01	/* Status Register A (r) */
#define	DART_CSRA	0x01	/* Clock-Select Register A (w) */
#define	DART_CRA	0x02	/* Command Register A (w) */
#define	DART_RBA	0x03	/* Receiver Buffer A (r) */
#define	DART_TBA	0x03	/* Transmitter Buffer A (w) */
#define	DART_IPCR	0x04	/* Input Port Change Register (r) */
#define	DART_ACR	0x04	/* Auxiliary Control Register (w) */
#define	DART_ISR	0x05	/* Interrupt Status Register (r) */
#define	DART_IMR	0x05	/* Interrupt Mask Register (w) */
#define	DART_CUR	0x06	/* Counter Mode Current MSB (r) */
#define	DART_CTUR	0x06	/* Counter/Timer Upper Register (w) */
#define	DART_CLR	0x07	/* Counter Mode Current LSB (r) */
#define	DART_CTLR	0x07	/* Counter/Timer Lower Register (w) */
#define	DART_MRB	0x08	/* Mode Register B (rw) */
#define	DART_SRB	0x09	/* Status Register B (r) */
#define	DART_CSRB	0x09	/* Clock-Select Register B (w) */
#define	DART_CRB	0x0a	/* Command Register B (w) */
#define	DART_RBB	0x0b	/* Receiver Buffer B (r) */
#define	DART_TBB	0x0b	/* Transmitter Buffer B (w) */
#define	DART_IVR	0x0c	/* Interrupt-Vector Register (rw) */
#define	DART_IP		0x0d	/* Input Port (r) */
#define	DART_OPCR	0x0d	/* Output Port Configuration Register (w) */
#define	DART_CTSTART	0x0e	/* Start-Counter Command (r) */
#define	DART_OPRS	0x0e	/* Output Port Bit Set Command (w) */
#define	DART_CTSTOP	0x0f	/* Stop-Counter Command (r) */
#define	DART_OPRR	0x0f	/* Output Port Bit Reset Command (w) */

#define	DART_SIZE	0x10

/*
 * Mode Register 1
 */

#define	DART_MR1_RX_RTR			0x80	/* OP0 set on RX RTR */
#define	DART_MR1_RX_IRQ_FFULL		0x40	/* RX interrupt on FIFO full */
#define	DART_MR1_RX_IRQ_RXRDY		0x00	/* RX interrupt on RxRDY */
#define	DART_MR1_ERROR_BLOCK		0x20	/* whole RX FIFO status */
#define	DART_MR1_ERROR_CHAR		0x00
#define	DART_MR1_PARITY_MASK		0x1c
#define	DART_MR1_PARITY_MULTI		0x18	/* multidrop mode */
#define	DART_MR1_PARITY_NONE		0x10	/* no parity */
#define	DART_MR1_PARITY_FORCED		0x08
#define	DART_MR1_PARITY_ENABLE		0x00
#define	DART_MR1_MULTIDROP_ADDRESS	0x04
#define	DART_MR1_MULTIDROP_DATA		0x00
#define	DART_MR1_PARITY_FORCED_HIGH	0x04
#define	DART_MR1_PARITY_FORCED_LOW	0x00
#define	DART_MR1_PARITY_ENABLE_ODD	0x04
#define	DART_MR1_PARITY_ENABLE_EVEN	0x00
#define	DART_MR1_BPC_MASK		0x03
#define	DART_MR1_BPC_8			0x03
#define	DART_MR1_BPC_7			0x02
#define	DART_MR1_BPC_6			0x01
#define	DART_MR1_BPC_5			0x00

/*
 * Mode Register 2
 */

#define	DART_MR2_MODE_MASK		0xc0
#define	DART_MR2_MODE_REMOTE_LOOPBACK	0xc0
#define	DART_MR2_MODE_LOCAL_LOOPBACK	0x80
#define	DART_MR2_MODE_ECHO		0x40
#define	DART_MR2_MODE_NORMAL		0x00
#define	DART_MR2_TX_RTS			0x20	/* OP0 reset on TX RTS */
#define	DART_MR2_TX_CTS			0x10	/* IP0 controls TX CTS */
#if 0 /* 68681 datasheet values */
#define	DART_MR2_STOP_MASK		0x0c
#define	DART_MR2_STOP_2			0x0c
#define	DART_MR2_STOP_15		0x08	/* 1.5 if async, 2 if sync */
#define	DART_MR2_STOP_1			0x04
#else /* 68692 datasheet values */
#define	DART_MR2_STOP_MASK		0x0f
#define	DART_MR2_STOP_2			0x0f
#define	DART_MR2_STOP_15		0x08	/* 1.5 if async, 2 if sync */
#define	DART_MR2_STOP_1			0x07
#define	DART_MR2_STOP_15_CL5		0x07
#define	DART_MR2_STOP_1_CL5		0x00
#endif

/*
 * Clock-Select Register
 */

#define	DART_CSR_RXCLOCK_MASK		0xf0
#define	DART_CSR_RXCLOCK_SHIFT		4
#define	DART_CSR_TXCLOCK_MASK		0x0f
#define	DART_CSR_TXCLOCK_SHIFT		0

#define	DART_CSR_50		0x00	/* set 1 */
#define	DART_CSR_75		0x00	/* set 2 */
#define	DART_CSR_110		0x01
#define	DART_CSR_134		0x02
#define	DART_CSR_150		0x03	/* set 2 */
#define	DART_CSR_200		0x03	/* set 1 */
#define	DART_CSR_300		0x04
#define	DART_CSR_600		0x05
#define	DART_CSR_1050		0x07	/* set 1 */
#define	DART_CSR_1200		0x06
#define	DART_CSR_1800		0x0a	/* set 2 */
#define	DART_CSR_2000		0x07	/* set 2 */
#define	DART_CSR_2400		0x08
#define	DART_CSR_4800		0x09
#define	DART_CSR_7200		0x0a	/* set 1 */
#define	DART_CSR_9600		0x0b
#define	DART_CSR_19200		0x0c	/* set 2 */
#define	DART_CSR_38400		0x0c	/* set 1 */
#define	DART_CSR_TIMER		0x0d
#define	DART_CSR_IP_16X		0x0e
#define	DART_CSR_IP_1X		0x0f

/* Input Port numbers for DART_CSR_IP_ settings */
#define	DART_IP_TXA	3
#define	DART_IP_RXA	4
#define	DART_IP_TXB	5
#define	DART_IP_RXB	2

/*
 * Command Register
 */

#define	DART_CR_STOP_BREAK		0x70
#define	DART_CR_START_BREAK		0x60
#define	DART_CR_RESET_BREAK		0x50
#define	DART_CR_RESET_ERROR		0x40
#define	DART_CR_RESET_TX		0x30
#define	DART_CR_RESET_RX		0x20
#define	DART_CR_RESET_MR1		0x10

#define	DART_CR_TX_DISABLE		0x08
#define	DART_CR_TX_ENABLE		0x04

#define	DART_CR_RX_DISABLE		0x02
#define	DART_CR_RX_ENABLE		0x01

/*
 * Status Register
 */

#define	DART_SR_BREAK			0x80	/* break received */
#define	DART_SR_FRAME			0x40	/* frame error */
#define	DART_SR_PARITY			0x20	/* parity error */
#define	DART_SR_OVERRUN			0x10
#define	DART_SR_TX_EMPTY		0x08
#define	DART_SR_TX_READY		0x04
#define	DART_SR_RX_FULL			0x02	/* RX FIFO full */
#define	DART_SR_RX_READY		0x01

/*
 * Output Port Configuration Register
 */

#define	DART_OPCR_TX_READY_B		0x80
#define	DART_OPCR_OP7			0x00
#define	DART_OPCR_TX_READY_A		0x40
#define	DART_OPCR_OP6			0x00
#define	DART_OPCR_RX_B			0x20
#define	DART_OPCR_OP5			0x00
#define	DART_OPCR_RX_A			0x10
#define	DART_OPCR_OP4			0x00

#define	DART_OPCR_RXCB			0x0c
#define	DART_OPCR_TXCB			0x08
#define	DART_OPCR_CT_OUTPUT		0x04
#define	DART_OPCR_OP3			0x00

#define	DART_OPCR_RXCA			0x03
#define	DART_OPCR_TXCA			0x02
#define	DART_OPCR_TXCA_X16		0x01
#define	DART_OPCR_OP2			0x00

/*
 * Auxiliary Control Register
 */

#define	DART_ACR_BRG_SET_MASK		0x80
#define	DART_ACR_BRG_SET_2		0x80
#define	DART_ACR_BRG_SET_1		0x00

#define	DART_ACR_CT_MASK		0x70
#define	DART_ACR_CT_TIMER_BIT		0x40
#define	DART_ACR_CT_TIMER_CLK_16	0x70	/* clock / 16 */
#define	DART_ACR_CT_TIMER_CLK		0x60	/* clock / 1 */
#define	DART_ACR_CT_TIMER_IP2_16	0x50	/* IP2 / 16 */
#define	DART_ACR_CT_TIMER_IP2		0x40	/* IP2 / 1 */
#define	DART_ACR_CT_COUNTER_CLK_16	0x30	/* clock / 16 */
#define	DART_ACR_CT_COUNTER_TXCB	0x20
#define	DART_ACR_CT_COUNTER_TXCA	0x10
#define	DART_ACR_CT_COUNTER_IP2		0x00	/* IP2 / 1 */

#define	DART_ACR_ISR_IP3_CHANGE_ENABLE	0x08
#define	DART_ACR_ISR_IP2_CHANGE_ENABLE	0x04
#define	DART_ACR_ISR_IP1_CHANGE_ENABLE	0x02
#define	DART_ACR_ISR_IP0_CHANGE_ENABLE	0x01

/*
 * Input Port Change Register
 */

#define	DART_IPCR_IP3_CHANGED		0x80
#define	DART_IPCR_IP2_CHANGED		0x40
#define	DART_IPCR_IP1_CHANGED		0x20
#define	DART_IPCR_IP0_CHANGED		0x10
#define	DART_IPCR_IP3_LEVEL		0x08
#define	DART_IPCR_IP2_LEVEL		0x04
#define	DART_IPCR_IP1_LEVEL		0x02
#define	DART_IPCR_IP0_LEVEL		0x01

/*
 * Interrupt Status Register / Interrupt Mask Register
 */

#define	DART_ISR_IP_CHANGE		0x80
#define	DART_ISR_DELTA_BREAK_B		0x40
#define	DART_ISR_RXB			0x20
#define	DART_ISR_TXB			0x10
#define	DART_ISR_CT			0x08
#define	DART_ISR_DELTA_BREAK_A		0x04
#define	DART_ISR_RXA			0x02
#define	DART_ISR_TXA			0x01

/*
 * Input Port
 */

#define	DART_IP_IACK			0x40	/* read only */
#define	DART_IP_IP5			0x20
#define	DART_IP_IP4			0x10	/* external BRG clock */
#define	DART_IP_IP3			0x08
#define	DART_IP_IP2			0x04
#define	DART_IP_IP1			0x02
#define	DART_IP_IP0			0x01

/*
 * Output Port
 */

#define	DART_OP_OP7			0x80
#define	DART_OP_OP6			0x40
#define	DART_OP_OP5			0x20
#define	DART_OP_OP4			0x10
#define	DART_OP_OP3			0x08
#define	DART_OP_OP2			0x04
#define	DART_OP_OP1			0x02
#define	DART_OP_OP0			0x01
