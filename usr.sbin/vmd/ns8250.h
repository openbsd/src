/*
 * Copyright (c) 2016 Mike Larkin <mlarkin@openbsd.org>
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
 * Emulated 8250 UART
 */
#define COM1_DATA	0x3f8
#define COM1_IER	0x3f9
#define COM1_IIR	0x3fa
#define COM1_LCR	0x3fb
#define COM1_MCR	0x3fc
#define COM1_LSR	0x3fd
#define COM1_MSR	0x3fe
#define COM1_SCR	0x3ff

/* ns8250 UART registers */
struct ns8250_regs {
	uint8_t lcr;	/* Line Control Register */
	uint8_t fcr;	/* FIFO Control Register */
	uint8_t iir;	/* Interrupt ID Register */
	uint8_t ier;	/* Interrupt Enable Register */
	uint8_t divlo;	/* Baud rate divisor low byte */
	uint8_t divhi;	/* Baud rate divisor high byte */
	uint8_t msr;	/* Modem Status Register */
	uint8_t lsr;	/* Line Status Register */
	uint8_t mcr;	/* Modem Control Register */
	uint8_t scr;	/* Scratch Register */
	uint8_t data;	/* Unread input data */
	int fd;		/* com port fd */
};

void ns8250_init(int);
uint8_t vcpu_exit_com(struct vm_run_params *);
void vcpu_process_com_data(union vm_exit *);
void vcpu_process_com_lcr(union vm_exit *);
void vcpu_process_com_lsr(union vm_exit *);
void vcpu_process_com_ier(union vm_exit *);
void vcpu_process_com_mcr(union vm_exit *);
void vcpu_process_com_iir(union vm_exit *);
void vcpu_process_com_msr(union vm_exit *);
void vcpu_process_com_scr(union vm_exit *);

/* XXX temporary until this is polled */
int vcpu_com1_needs_intr(void);
