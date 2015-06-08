/*	$OpenBSD: pl011reg.h,v 1.1 2015/06/08 06:33:16 jsg Exp $	*/

/*
 * Copyright (c) 2014 Patrick Wildt <patrick@blueri.se>
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

#ifndef	PL011REG_H
#define	PL011REG_H

#define		UART_DR				0x00 /* Data register */
#define		UART_DR_DATA(x)			((x) & 0xf)
#define		UART_DR_FE			(1 << 8) /* Framing error */
#define		UART_DR_PE			(1 << 9) /* Parity error */
#define		UART_DR_BE			(1 << 10) /* Break error */
#define		UART_DR_OE			(1 << 11) /* Overrun error */
#define		UART_RSR			0x04 /* Receive status register */
#define		UART_RSR_FE			(1 << 0) /* Framing error */
#define		UART_RSR_PE			(1 << 1) /* Parity error */
#define		UART_RSR_BE			(1 << 2) /* Break error */
#define		UART_RSR_OE			(1 << 3) /* Overrun error */
#define		UART_ECR			0x04 /* Error clear register */
#define		UART_ECR_FE			(1 << 0) /* Framing error */
#define		UART_ECR_PE			(1 << 1) /* Parity error */
#define		UART_ECR_BE			(1 << 2) /* Break error */
#define		UART_ECR_OE			(1 << 3) /* Overrun error */
#define		UART_FR				0x18 /* Flag register */
#define		UART_FR_CTS			(1 << 0) /* Clear to send */
#define		UART_FR_DSR			(1 << 1) /* Data set ready */
#define		UART_FR_DCD			(1 << 2) /* Data carrier detect */
#define		UART_FR_BUSY			(1 << 3) /* UART busy */
#define		UART_FR_RXFE			(1 << 4) /* Receive FIFO empty */
#define		UART_FR_TXFF			(1 << 5) /* Transmit FIFO full */
#define		UART_FR_RXFF			(1 << 6) /* Receive FIFO full */
#define		UART_FR_TXFE			(1 << 7) /* Transmit FIFO empty */
#define		UART_FR_RI			(1 << 8) /* Ring indicator */
#define		UART_ILPR			0x20 /* IrDA low-power counter register */
#define		UART_ILPR_ILPDVSR		((x) & 0xf) /* IrDA low-power divisor */
#define		UART_IBRD			0x24 /* Integer baud rate register */
#define		UART_IBRD_DIVINT		((x) & 0xff) /* Integer baud rate divisor */
#define		UART_FBRD			0x28 /* Fractional baud rate register */
#define		UART_FBRD_DIVFRAC		((x) & 0x3f) /* Fractional baud rate divisor */
#define		UART_LCR_H			0x2c /* Line control register */
#define		UART_LCR_H_BRK			(1 << 0) /* Send break */
#define		UART_LCR_H_PEN			(1 << 1) /* Parity enable */
#define		UART_LCR_H_EPS			(1 << 2) /* Even parity select */
#define		UART_LCR_H_STP2			(1 << 3) /* Two stop bits select */
#define		UART_LCR_H_FEN			(1 << 4) /* Enable FIFOs */
#define		UART_LCR_H_WLEN5		(0x0 << 5) /* Word length: 5 bits */
#define		UART_LCR_H_WLEN6		(0x1 << 5) /* Word length: 6 bits */
#define		UART_LCR_H_WLEN7		(0x2 << 5) /* Word length: 7 bits */
#define		UART_LCR_H_WLEN8		(0x3 << 5) /* Word length: 8 bits */
#define		UART_LCR_H_SPS			(1 << 7) /* Stick parity select */
#define		UART_CR				0x30 /* Control register */
#define		UART_CR_UARTEN			(1 << 0) /* UART enable */
#define		UART_CR_SIREN			(1 << 1) /* SIR enable */
#define		UART_CR_SIRLP			(1 << 2) /* IrDA SIR low power mode */
#define		UART_CR_LBE			(1 << 7) /* Loop back enable */
#define		UART_CR_TXE			(1 << 8) /* Transmit enable */
#define		UART_CR_RXE			(1 << 9) /* Receive enable */
#define		UART_CR_DTR			(1 << 10) /* Data transmit enable */
#define		UART_CR_RTS			(1 << 11) /* Request to send */
#define		UART_CR_OUT1			(1 << 12)
#define		UART_CR_OUT2			(1 << 13)
#define		UART_CR_CTSE			(1 << 14) /* CTS hardware flow control enable */
#define		UART_CR_RTSE			(1 << 15) /* RTS hardware flow control enable */
#define		UART_IFLS			0x34 /* Interrupt FIFO level select register */
#define		UART_IMSC			0x38 /* Interrupt mask set/clear register */
#define		UART_IMSC_RIMIM			(1 << 0)
#define		UART_IMSC_CTSMIM		(1 << 1)
#define		UART_IMSC_DCDMIM		(1 << 2)
#define		UART_IMSC_DSRMIM		(1 << 3)
#define		UART_IMSC_RXIM			(1 << 4)
#define		UART_IMSC_TXIM			(1 << 5)
#define		UART_IMSC_RTIM			(1 << 6)
#define		UART_IMSC_FEIM			(1 << 7)
#define		UART_IMSC_PEIM			(1 << 8)
#define		UART_IMSC_BEIM			(1 << 9)
#define		UART_IMSC_OEIM			(1 << 10)
#define		UART_RIS			0x3c /* Raw interrupt status register */
#define		UART_MIS			0x40 /* Masked interrupt status register */
#define		UART_ICR			0x44 /* Interrupt clear register */
#define		UART_DMACR			0x48 /* DMA control register */
#define		UART_SPACE			0x100

#endif /* !PL011REG_H */
