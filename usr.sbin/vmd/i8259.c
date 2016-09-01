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

#include <string.h>

#include <sys/types.h>

#include <dev/isa/isareg.h>

#include <machine/vmmvar.h>

#include "proc.h"
#include "i8259.h"

struct i8259 {
	uint8_t irr;
	uint8_t imr;
	uint8_t isr;
	uint8_t smm;
	uint8_t poll;
	uint8_t cur_icw;
	uint8_t init_mode;
	uint8_t vec;
	uint8_t irq_conn;
	uint8_t next_ocw_read;
	uint8_t auto_eoi;
	uint8_t rotate_auto_eoi;
	uint8_t lowest_pri;
	uint8_t asserted;
};

#define PIC_IRR 0
#define PIC_ISR 1

/* Master and slave PICs */
struct i8259 pics[2];

/*
 * i8259_init
 *
 * Initialize the emulated i8259 PIC.
 */
void
i8259_init(void)
{
	memset(&pics, 0, sizeof(pics));
	pics[MASTER].cur_icw = 1;
	pics[SLAVE].cur_icw = 1;
}

/*
 * i8259_is_pending
 *
 * Determine if an IRQ is pending on either the slave or master PIC.
 *
 * Return Values:
 *  1 if an IRQ (any IRQ) is pending, 0 otherwise
 */
uint8_t
i8259_is_pending(void)
{
	uint8_t pending = 0;
	uint8_t master_pending;
	uint8_t slave_pending;

	master_pending = pics[MASTER].irr & ~(pics[MASTER].imr | (1 << 2));
	slave_pending = pics[SLAVE].irr & ~pics[SLAVE].imr;

	if (master_pending || slave_pending)
		pending = 1;

	return pending;
}	

/*
 * i8259_ack
 *
 * This function is called when the vcpu exits and is ready to accept an
 * interrupt.
 *
 * Return values:
 *  interrupt vector to inject, 0xFFFF if no irq pending
 */
uint16_t
i8259_ack(void)
{
	uint8_t high_prio_m, high_prio_s;
	uint8_t i;
	uint16_t ret;

	ret = 0xFFFF;

	if (pics[MASTER].asserted == 0 && pics[SLAVE].asserted == 0)
		return (ret);

	high_prio_m = pics[MASTER].lowest_pri + 1;
	if (high_prio_m > 7)
		high_prio_m = 0;

	high_prio_s = pics[SLAVE].lowest_pri + 1;
	if (high_prio_s > 7)
		high_prio_s = 0;

	i = high_prio_m;
	do {
		if ((pics[MASTER].irr & (1 << i)) && i != 2 &&
		    !(pics[MASTER].imr & (1 << i))) {
			/* Master PIC has highest prio and ready IRQ */
			pics[MASTER].irr &= ~(1 << i);
			pics[MASTER].isr |= (1 << i);

			if (pics[MASTER].irr == 0)
				pics[MASTER].asserted = 0;

			ret = i;

			/* XXX - intr still needs |= 0xFF00 ?? */
			if (pics[MASTER].irr || pics[SLAVE].irr)
				ret |= 0xFF00;

			return ret;
		}

		i++;

		if (i > 7)
			i = 0;

	} while (i != high_prio_m);

	i = high_prio_s;
	do {
		if ((pics[SLAVE].irr & (1 << i)) &&
		    !(pics[SLAVE].imr & (1 << i))) {
			/* Slave PIC has highest prio and ready IRQ */
			pics[SLAVE].irr &= ~(1 << i);
			pics[MASTER].irr &= ~(1 << 2);

			pics[SLAVE].isr |= (1 << i);
			pics[MASTER].isr |= (1 << 2);

			if (pics[SLAVE].irr == 0) {
				pics[SLAVE].asserted = 0;
				if (pics[MASTER].irr == 0)
					pics[MASTER].asserted = 0;
			}

			ret = i + 8;
			/* XXX - intr still needs |= 0xFF00 ?? */
			if ((pics[MASTER].irr & ~0x4) || (pics[SLAVE].irr))
				ret |= 0xFF00;

			return ret;
		}

		i++;

		if (i > 7)
			i = 0;
	} while (i != high_prio_s);

	return (0xFFFF);
}

/*
 * i8259_assert_irq
 *
 * Asserts the IRQ specified
 *
 * Parameters:
 *  irq: the IRQ to assert
 */
void
i8259_assert_irq(uint8_t irq)
{
	if (irq <= 7) {
		if (pics[MASTER].imr & (1 << irq))
			return;

		pics[MASTER].irr |= (1 << irq);
		pics[MASTER].asserted = 1;
	} else {
		if (pics[SLAVE].imr & (1 << (irq - 8)))
			return;

		pics[SLAVE].irr |= (1 << (irq - 8));
		pics[SLAVE].asserted = 1;

		/* Assert cascade IRQ on master PIC */
		pics[MASTER].irr |= (1 << 2);
		pics[MASTER].asserted = 1;
	}
}	

/*
 * i8259_deassert_irq
 *
 * Deasserts the IRQ specified
 *
 * Parameters:
 *  irq: the IRQ to deassert
 */
void
i8259_deassert_irq(uint8_t irq)
{
	if (irq <= 7)
		pics[MASTER].irr &= ~(1 << irq);
	else {
		pics[SLAVE].irr &= ~(1 << (irq - 8));

		/* Deassert cascade IRQ on master if no IRQs on slave */
		if (pics[SLAVE].irr == 0)
			pics[MASTER].irr &= ~(1 << 2);
	}
}

/*
 * i8259_write_datareg
 *
 * Write to a specified data register in the emulated PIC during PIC
 * initialization. The data write follows the state model in the i8259 (in
 * other words, data is expected to be written in a specific order).
 *
 * Parameters:
 *  n: PIC to write to (MASTER/SLAVE)
 *  data: data to write
 */
static void
i8259_write_datareg(uint8_t n, uint8_t data)
{
	struct i8259 *pic = &pics[n];

	if (pic->init_mode == 1) {
		if (pic->cur_icw == 2) {
			/* Set vector */
			pic->vec = data;
		} else if (pic->cur_icw == 3) {
			/* Set IRQ interconnects */
			if (n == SLAVE && (data & 0xf8)) {
				log_warn("%s: pic %d invalid icw2 0x%x",
				    __func__, n, data);
				return;
			}
			pic->irq_conn = data;
		} else if (pic->cur_icw == 4) {
			if (!(data & ICW4_UP)) {
				log_warn("%s: pic %d init error: x86 bit "
				    "clear", __func__, n);
				return;
			}
	
			if (data & ICW4_AEOI) {
				log_warn("%s: pic %d: aeoi mode set",
				    __func__, n);
				pic->auto_eoi = 1;
				return;
			}
	
			if (data & ICW4_MS) {
				log_warn("%s: pic %d init error: M/S mode",
				    __func__, n);
				return;
			}
	
			if (data & ICW4_BUF) {
				log_warn("%s: pic %d init error: buf mode",
				    __func__, n);
				return;
			}
	
			if (data & 0xe0) {
				log_warn("%s: pic %d init error: invalid icw4 "
				    " 0x%x", __func__, n, data);
				return;
			}
		}	

		pic->cur_icw++;
		if (pic->cur_icw == 5) {
			pic->cur_icw = 1;
			pic->init_mode = 0;
		}
	} else
		pic->imr = data;
}

/*
 * i8259_specific_eoi
 *
 * Handles specific end of interrupt commands
 *
 * Parameters:
 *  n: PIC to deliver this EOI to
 *  data: interrupt to EOI
 */
static void
i8259_specific_eoi(uint8_t n, uint8_t data)
{
	uint8_t oldisr;

	if (!(pics[n].isr & (1 << (data & 0x7)))) {
		log_warn("%s: pic %d specific eoi irq %d while not in"
		    " service", __func__, n, (data & 0x7));
	}

	oldisr = pics[n].isr;
	pics[n].isr &= ~(1 << (data & 0x7));
}

/*
 * i8259_nonspecific_eoi
 *
 * Handles nonspecific end of interrupt commands
 * XXX not implemented
 */
static void
i8259_nonspecific_eoi(uint8_t n, uint8_t data)
{
	log_warn("%s: pic %d nonspecific eoi not supported", __func__, n);
}

/*
 * i8259_rotate_priority
 *
 * Rotates the interrupt priority on the specified PIC
 *
 * Parameters:
 *  n: PIC whose priority should be rotated
 */
static void
i8259_rotate_priority(uint8_t n)
{
	pics[n].lowest_pri++;
	if (pics[n].lowest_pri > 7)
		pics[n].lowest_pri = 0;
}

/*
 * i8259_write_cmdreg
 *
 * Write to the PIC command register
 *
 * Parameters:
 *  n: PIC whose command register should be written to
 *  data: data to write
 */ 
static void
i8259_write_cmdreg(uint8_t n, uint8_t data)
{
	struct i8259 *pic = &pics[n];

	if (data & ICW1_INIT) {
		/* Validate init params */
		if (!(data & ICW1_ICW4)) {
			log_warn("%s: pic %d init error: no ICW4 request",
			    __func__, n);
			return;
		}

		if (data & (ICW1_IVA1 | ICW1_IVA2 | ICW1_IVA3)) {
			log_warn("%s: pic %d init error: IVA specified",
			    __func__, n);
			return;
		}

		if (data & ICW1_SNGL) {
			log_warn("%s: pic %d init error: single pic mode",
			    __func__, n);
			return;
		}

		if (data & ICW1_ADI) {
			log_warn("%s: pic %d init error: address interval",
			    __func__, n);
			return;
		}

		if (data & ICW1_LTIM) {
			log_warn("%s: pic %d init error: level trigger mode",
			    __func__, n);
			return;
		}

		pic->init_mode = 1;
		pic->cur_icw = 2;
		pic->imr = 0;
		pic->isr = 0;
		pic->irr = 0;
		pic->asserted = 0;
		pic->lowest_pri = 7;
		pic->rotate_auto_eoi = 0;
		return;
	} else if (data & OCW_SELECT) {
			/* OCW3 */
			if (data & OCW3_ACTION) {
				if (data & OCW3_RR) {
					if (data & OCW3_RIS)
						pic->next_ocw_read = PIC_ISR;
					else
						pic->next_ocw_read = PIC_IRR;
				}
			}

			if (data & OCW3_SMACTION) {
				if (data & OCW3_SMM) {
					pic->smm = 1;
					/* XXX update intr here */
				} else
					pic->smm = 0;
			}

			if (data & OCW3_POLL) {
				pic->poll = 1;
				/* XXX update intr here */
			}

			return;
	} else {
		/* OCW2 */
		if (data & OCW2_EOI) {
			/*
			 * An EOI command was received. It could be one of
			 * several different varieties:
			 *
			 * Nonspecific EOI (0x20)
			 * Specific EOI (0x60..0x67)
			 * Nonspecific EOI + rotate (0xA0)
			 * Specific EOI + rotate (0xE0..0xE7)
			 */
			switch (data) {
			case OCW2_EOI:
				i8259_nonspecific_eoi(n, data);
				break;
			case OCW2_SEOI ... OCW2_SEOI + 7:
				i8259_specific_eoi(n, data);
				break;
			case OCW2_ROTATE_NSEOI:
				i8259_nonspecific_eoi(n, data);
				i8259_rotate_priority(n);
				break;
			case OCW2_ROTATE_SEOI ... OCW2_ROTATE_SEOI + 7:
				i8259_specific_eoi(n, data);
				i8259_rotate_priority(n);
				break;
			}
			return;
		}

		if (data == OCW2_NOP)
			return;

		if ((data & OCW2_SET_LOWPRIO) == OCW2_SET_LOWPRIO) {
			/* Set low priority value (bits 0-2) */
			pic->lowest_pri = data & 0x7;
			return;
		}

		if (data == OCW2_ROTATE_AEOI_CLEAR) {
			pic->rotate_auto_eoi = 0;
			return;
		}

		if (data == OCW2_ROTATE_AEOI_SET) {
			pic->rotate_auto_eoi = 1;
			return;
		}

		return;
	}
}

/*
 * i8259_read_datareg
 *
 * Read the PIC's IMR
 *
 * Parameters:
 *  n: PIC to read
 *
 * Return value:
 *  selected PIC's IMR
 */
static uint8_t
i8259_read_datareg(uint8_t n)
{
	struct i8259 *pic = &pics[n];

	return (pic->imr);
}

/*
 * i8259_read_cmdreg
 *
 * Read the PIC's IRR or ISR, depending on the current PIC mode (value
 * selected via the OCW3 command)
 *
 * Parameters:
 *  n: PIC to read
 *
 * Return value:
 *  selected PIC's IRR/ISR
 */
static uint8_t
i8259_read_cmdreg(uint8_t n)
{
	struct i8259 *pic = &pics[n];

	if (pic->next_ocw_read == PIC_IRR)
		return (pic->irr);
	else if (pic->next_ocw_read == PIC_ISR)
		return (pic->isr);

	fatal("%s: invalid PIC config during cmdreg read", __func__);
}

/*
 * i8259_io_write
 *
 * Callback to handle write I/O to the emulated PICs in the VM
 *
 * Parameters:
 *  vei: vm exit info for this I/O
 */
static void
i8259_io_write(union vm_exit *vei)
{
	uint16_t port = vei->vei.vei_port;
	uint8_t data = vei->vei.vei_data;
	uint8_t n = 0;

	switch (port) {
	case IO_ICU1:
	case IO_ICU1 + 1:
		n = MASTER;
		break;
	case IO_ICU2:
	case IO_ICU2 + 1:
		n = SLAVE;
		break;
	default:
		fatal("%s: invalid port 0x%x", __func__, port);
	}

	if (port == IO_ICU1 + 1 || port == IO_ICU2 + 1)
		i8259_write_datareg(n, data);
	else
		i8259_write_cmdreg(n, data);
	
}

/*
 * i8259_io_read
 *
 * Callback to handle read I/O to the emulated PICs in the VM
 *
 * Parameters:
 *  vei: vm exit info for this I/O
 *
 * Return values:
 *  data that was read, based on the port information in 'vei'
 */
static uint8_t
i8259_io_read(union vm_exit *vei)
{
	uint16_t port = vei->vei.vei_port;
	uint8_t n = 0;

	switch (port) {
	case IO_ICU1:
	case IO_ICU1 + 1:
		n = MASTER;
		break;
	case IO_ICU2:
	case IO_ICU2 + 1:
		n = SLAVE;
		break;
	default:
		fatal("%s: invalid port 0x%x", __func__, port);
	}

	if (port == IO_ICU1 + 1 || port == IO_ICU2 + 1)
		return i8259_read_datareg(n);
	else
		return i8259_read_cmdreg(n);
}

/*
 * vcpu_exit_i8259
 *
 * Top level exit handler for PIC operations
 *
 * Parameters:
 *  vrp: VCPU run parameters (contains exit information) for this PIC operation
 *
 * Return value:
 *  Always 0xFF (PIC read/writes don't generate interrupts directly)
 */
uint8_t
vcpu_exit_i8259(struct vm_run_params *vrp)
{
	union vm_exit *vei = vrp->vrp_exit;

	if (vei->vei.vei_dir == 0) {
		i8259_io_write(vei);
	} else {
		vei->vei.vei_data = i8259_io_read(vei);
	}

	return (0xFF);
}
