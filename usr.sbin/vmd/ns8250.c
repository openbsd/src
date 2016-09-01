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

#include <sys/types.h>

#include <dev/ic/comreg.h>

#include <machine/vmmvar.h>

#include <string.h>
#include <unistd.h>

#include "ns8250.h"
#include "proc.h"

extern char *__progname;
struct ns8250_regs com1_regs;

void
ns8250_init(int fd)
{
	memset(&com1_regs, 0, sizeof(struct ns8250_regs));
	com1_regs.fd = fd;
}

/*
 * vcpu_process_com_data
 *
 * Emulate in/out instructions to the com1 (ns8250) UART data register
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_data(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * The guest wrote to the data register. Since we are emulating a
	 * no-fifo chip, write the character immediately to the pty and
	 * assert TXRDY in IIR (if the guest has requested TXRDY interrupt
	 * reporting)
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		write(com1_regs.fd, &vei->vei.vei_data, 1);
		if (com1_regs.ier & 0x2) {
			/* Set TXRDY */
			com1_regs.iir |= IIR_TXRDY;
			/* Set "interrupt pending" (IIR low bit cleared) */
			com1_regs.iir &= ~0x1;
		}
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * The guest read from the data register. Check to see if
		 * there is data available (RXRDY) and if so, consume the
		 * input data and return to the guest. Also clear the
		 * interrupt info register regardless.
		 */
		if (com1_regs.lsr & LSR_RXRDY) {
			vei->vei.vei_data = com1_regs.data;
			com1_regs.data = 0x0;
			com1_regs.lsr &= ~LSR_RXRDY;
		} else {
			/* XXX should this be com1_regs.data or 0xff? */
			vei->vei.vei_data = com1_regs.data;
			log_warnx("guest reading com1 when not ready");
		}

		/* Reading the data register always clears RXRDY from IIR */
		com1_regs.iir &= ~IIR_RXRDY;

		/*
		 * Clear "interrupt pending" by setting IIR low bit to 1
		 * if no interrupt are pending
		 */
		if (com1_regs.iir == 0x0)
			com1_regs.iir = 0x1;
	}
}

/*
 * vcpu_process_com_lcr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART line control register
 *
 * Paramters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_lcr(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * Write content to line control register
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		com1_regs.lcr = (uint8_t)vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * Read line control register
		 */
		vei->vei.vei_data = com1_regs.lcr;
	}
}

/*
 * vcpu_process_com_iir
 *
 * Emulate in/out instructions to the com1 (ns8250) UART interrupt information
 * register. Note that writes to this register actually are to a different
 * register, the FCR (FIFO control register) that we don't emulate but still
 * consume the data provided.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_iir(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * Write to FCR
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		com1_regs.fcr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * Read IIR. Reading the IIR resets the TXRDY bit in the IIR
		 * after the data is read.
		 */
		vei->vei.vei_data = com1_regs.iir;
		com1_regs.iir &= ~IIR_TXRDY;

		/*
		 * Clear "interrupt pending" by setting IIR low bit to 1
		 * if no interrupts are pending
		 */
		if (com1_regs.iir == 0x0)
			com1_regs.iir = 0x1;
	}
}

/*
 * vcpu_process_com_mcr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART modem control
 * register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_mcr(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * Write to MCR
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		com1_regs.mcr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * Read from MCR
		 */
		vei->vei.vei_data = com1_regs.mcr;
	}
}

/*
 * vcpu_process_com_lsr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART line status register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_lsr(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * Write to LSR. This is an illegal operation, so we just log it and
	 * continue.
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		log_warnx("%s: LSR UART write 0x%x unsupported",
		    __progname, vei->vei.vei_data);
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * Read from LSR. We always report TXRDY and TSRE since we
		 * can process output characters immediately (at any time).
		 */
		vei->vei.vei_data = com1_regs.lsr | LSR_TSRE | LSR_TXRDY;
	}
}

/*
 * vcpu_process_com_msr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART modem status register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_msr(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * Write to MSR. This is an illegal operation, so we just log it and
	 * continue.
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		log_warnx("%s: MSR UART write 0x%x unsupported",
		    __progname, vei->vei.vei_data);
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * Read from MSR. We always report DCD, DSR, and CTS.
		 */
		vei->vei.vei_data = com1_regs.lsr | MSR_DCD | MSR_DSR | MSR_CTS;
	}
}

/*
 * vcpu_process_com_scr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART scratch register. The
 * scratch register is sometimes used to distinguish an 8250 from a 16450,
 * and/or used to distinguish submodels of the 8250 (eg 8250A, 8250B). We
 * simulate an "original" 8250 by forcing the scratch register to return data
 * on read that is different from what was written.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_scr(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * Write to SCR
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		com1_regs.scr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * Read from SCR. To make sure we don't accidentally simulate
		 * a real scratch register, we negate what was written on
		 * subsequent readback.
		 */
		vei->vei.vei_data = ~com1_regs.scr;
	}
}

/*
 * vcpu_process_com_ier
 *
 * Emulate in/out instructions to the com1 (ns8250) UART interrupt enable
 * register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_ier(union vm_exit *vei)
{
	/*
	 * vei_dir == VEI_DIR_OUT : out instruction
	 *
	 * Write to IER
	 */
	if (vei->vei.vei_dir == VEI_DIR_OUT) {
		com1_regs.ier = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == VEI_DIR_IN : in instruction
		 *
		 * Read from IER
		 */
		vei->vei.vei_data = com1_regs.ier;
	}
}

/*
 * vcpu_exit_com
 *
 * Process com1 (ns8250) UART exits. vmd handles most basic 8250
 * features with the exception of the divisor latch (eg, no baud
 * rate support)
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 *
 * Return value:
 *  Interrupt to inject to the guest VM, or 0xFF if no interrupt should
 *      be injected.
 */
uint8_t
vcpu_exit_com(struct vm_run_params *vrp)
{
	union vm_exit *vei = vrp->vrp_exit;

	switch (vei->vei.vei_port) {
	case COM1_LCR:
		vcpu_process_com_lcr(vei);
		break;
	case COM1_IER:
		vcpu_process_com_ier(vei);
		break;
	case COM1_IIR:
		vcpu_process_com_iir(vei);
		break;
	case COM1_MCR:
		vcpu_process_com_mcr(vei);
		break;
	case COM1_LSR:
		vcpu_process_com_lsr(vei);
		break;
	case COM1_MSR:
		vcpu_process_com_msr(vei);
		break;
	case COM1_SCR:
		vcpu_process_com_scr(vei);
		break;
	case COM1_DATA:
		vcpu_process_com_data(vei);
		break;
	}

	return (0xFF);
}

/* XXX temporary until this is polled */
int
vcpu_com1_needs_intr(void)
{
	int need_irq = 0;
	ssize_t sz;
	char ch;

	/*
	 * Is there a new character available on com1?
	 * If so, consume the character, buffer it into the com1 data register
	 * assert IRQ4, and set the line status register RXRDY bit.
	 *
	 * XXX - move all this com intr checking to another function using poll
	 */
	sz = read(com1_regs.fd, &ch, sizeof(char));
	if (sz == 1) {
		com1_regs.lsr |= LSR_RXRDY;
		com1_regs.data = ch;
		/* XXX these ier and iir bits should be IER_x and IIR_x */
		if (com1_regs.ier & 0x1) {
			com1_regs.iir |= (2 << 1);
			com1_regs.iir &= ~0x1;
		}
	}

	/*
	 * Clear "interrupt pending" by setting IIR low bit to 1 if no
	 * interrupts are pending
	 */
	/* XXX these iir magic numbers should be IIR_x */
	if ((com1_regs.iir & ~0x1) == 0x0)
		com1_regs.iir = 0x1;

	/* If pending interrupt, inject */
	if ((com1_regs.iir & 0x1) == 0)
		need_irq = 1;

	return need_irq;
}
