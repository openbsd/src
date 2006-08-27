/*	$OpenBSD: qscvar.h,v 1.1 2006/08/27 16:55:41 miod Exp $	*/
/*
 * Copyright (c) 2006 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Logical, per port, registers (serial functions only)
 */

#define	SC_MR		0x00	/* R/W	Mode Register */
#define	SC_SR		0x01	/* R	Status Register */
#define	SC_CSR		0x01	/* W	Clock Select Register */
#define	SC_CR		0x02	/* W	Command Register */
#define	SC_RXFIFO	0x03	/* R	Receive Holding Register */
#define	SC_TXFIFO	0x03	/* W	Transmit Holding Register */
#define	SC_IPCR		0x04	/* R	Input Port Change Register */
#define	SC_ACR		0x04	/* W	Auxiliary Control Register */
#define	SC_ISR		0x05	/* R	Interrupt Status Register */
#define	SC_IMR		0x05	/* W	Interrupt Mask Register */
#define	SC_OPR		0x06	/* R/W	Output Port Register */
#define	SC_IPR		0x07	/* R	Input Port Register */
#define	SC_IOPCR	0x07	/* W	I/O Port Control Register */
#define	SC_LOGICAL	0x08

#define	SC_NLINES	4

/* saved registers */
struct qsc_sv_reg {
	u_int8_t	sv_mr1[SC_NLINES];
	u_int8_t	sv_mr2[SC_NLINES];
	u_int8_t	sv_csr[SC_NLINES];
	u_int8_t	sv_cr[SC_NLINES];
	u_int8_t	sv_imr[SC_NLINES / 2];
};

struct qsc_input_hook {
	int	(*fn)(void *, int);
	void	*arg;
};

struct qscsoftc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_addr_t		sc_regaddr[SC_NLINES][SC_LOGICAL];

	int			sc_console;
	int			sc_rdy;

	struct qsc_sv_reg	*sc_sv_reg;
	struct qsc_sv_reg	sc_sv_reg_storage;

	struct tty		*sc_tty[SC_NLINES];
	u_int		 	sc_swflags[SC_NLINES];

	struct qsc_input_hook	sc_hook[SC_NLINES];
};

/*
 * Line assignments on the VXT2000
 */

#define	QSC_LINE_SERIAL		0
#define	QSC_LINE_DEAD		1
#define	QSC_LINE_KEYBOARD	2
#define	QSC_LINE_MOUSE		3

struct	qsc_attach_args {
	u_int	qa_line;
	int	qa_console;	/* for keyboard attachment */
	struct qsc_input_hook *qa_hook;
};

int	qscgetc(u_int);
void	qscputc(u_int, int);

int	qsckbd_cnattach(u_int);
