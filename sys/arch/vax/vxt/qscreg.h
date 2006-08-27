/*	$OpenBSD: qscreg.h,v 1.1 2006/08/27 16:55:41 miod Exp $	*/
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
 * SC26C94 registers
 */

#define	SC_MRA		0x00	/* R/W	Mode Register A */
#define	SC_SRA		0x01	/* R	Status Register A */
#define	SC_CSRA		0x01	/* W	Clock Select Register A */
#define	SC_CRA		0x02	/* W	Command Register A */
#define	SC_RXFIFOA	0x03	/* R	Receive Holding Register A */
#define	SC_TXFIFOA	0x03	/* W	Transmit Holding Register A */
#define	SC_IPCRAB	0x04	/* R	Input Port Change Register AB */
#define	SC_ACRAB	0x04	/* W	Auxiliary Control Register AB */
#define	SC_ISRAB	0x05	/* R	Interrupt Status Register AB */
#define	SC_IMRAB	0x05	/* W	Interrupt Mask Register AB */
#define	SC_CTURAB	0x06	/* R/W	Counter/Timer Upper Register AB */
#define	SC_CTLRAB	0x07	/* R/W	Counter/Timer Lower Register AB */
#define	SC_MRB		0x08	/* R/W	Mode Register A */
#define	SC_SRB		0x09	/* R	Status Register A */
#define	SC_CSRB		0x09	/* W	Clock Select Register A */
#define	SC_CRB		0x0a	/* W	Command Register A */
#define	SC_RXFIFOB	0x0b	/* R	Receive Holding Register A */
#define	SC_TXFIFOB	0x0b	/* W	Transmit Holding Register A */
#define	SC_OPRAB	0x0c	/* R/W	Output Port Register AB */
#define	SC_IPRAB	0x0d	/* R	Input Port Register AB */
#define	SC_IOPCRA	0x0d	/* W	I/O Port Control Register A */
#define	SC_IOPCRB	0x0e	/* W	I/O Port Control Register A */
#define	SC_CTSTARTAB	0x0e	/* R	Start Counter AB */
#define	SC_CTSTOPAB	0x0f	/* R	Start Counter AB */

#define	SC_MRC		0x10	/* R/W	Mode Register C */
#define	SC_SRC		0x11	/* R	Status Register C */
#define	SC_CSRC		0x11	/* W	Clock Select Register C */
#define	SC_CRC		0x12	/* W	Command Register C */
#define	SC_RXFIFOC	0x13	/* R	Receive Holding Register C */
#define	SC_TXFIFOC	0x13	/* W	Transmit Holding Register C */
#define	SC_IPCRCD	0x14	/* R	Input Port Change Register CD */
#define	SC_ACRCD	0x14	/* W	Auxiliary Control Register CD */
#define	SC_ISRCD	0x15	/* R	Interrupt Status Register CD */
#define	SC_IMRCD	0x15	/* W	Interrupt Mask Register CD */
#define	SC_CTURCD	0x16	/* R/W	Counter/Timer Upper Register CD */
#define	SC_CTLRCD	0x17	/* R/W	Counter/Timer Lower Register CD */
#define	SC_MRD		0x18	/* R/W	Mode Register C */
#define	SC_SRD		0x19	/* R	Status Register C */
#define	SC_CSRD		0x19	/* W	Clock Select Register C */
#define	SC_CRD		0x1a	/* W	Command Register C */
#define	SC_RXFIFOD	0x1b	/* R	Receive Holding Register C */
#define	SC_TXFIFOD	0x1b	/* W	Transmit Holding Register C */
#define	SC_OPRCD	0x1c	/* R/W	Output Port Register CD */
#define	SC_IPRCD	0x1d	/* R	Input Port Register CD */
#define	SC_IOPCRC	0x1d	/* W	I/O Port Control Register C */
#define	SC_IOPCRD	0x1e	/* W	I/O Port Control Register C */
#define	SC_CTSTARTCD	0x1e	/* R	Start Counter CD */
#define	SC_CTSTOPCD	0x1f	/* R	Start Counter CD */

#define	SC_BIDCRA	0x20	/* R/W	Bidding Control Register A */
#define	SC_BIDCRB	0x21	/* R/W	Bidding Control Register B */
#define	SC_BIDCRC	0x22	/* R/W	Bidding Control Register C */
#define	SC_BIDCRD	0x23	/* R/W	Bidding Control Register D */
#define	SC_POWERDOWN	0x24	/* W	Power Down */
#define	SC_POWERUP	0x25	/* W	Power Up */
#define	SC_DDACKN	0x26	/* W	Disable DACKN */
#define	SC_EDACKN	0x27	/* W	Enable DACKN */

#define	SC_CIR		0x28	/* R	Current Interrupt Register */
#define	SC_GICR		0x29	/* R	Global Interrupting Channel Register */
#define	SC_IVR		0x29	/* W	Interrupt Vector Register */
#define	SC_GBICR	0x2a	/* R	Global Int Byte Count Register */
#define	SC_UCIR		0x2a	/* W	Update CIR */
#define	SC_GRXFIFO	0x2b	/* R	Global Receive Holding Register */
#define	SC_GTXFIFO	0x2b	/* W	Global Transmit Holding Register */
#define	SC_ICR		0x2c	/* R/W	Interrupt Control Register */
#define	SC_BRGRATE	0x2d	/* W	BRG Rate */
#define	SC_X1DIV2	0x2e	/* W	Set X1/CLK divide by two */
#define	SC_X1NORM	0x2f	/* W	Set X1/CLK normal */

/* mode register 1: MR1x operations */
#define RXRTS        0x80  /* enable receiver RTS */
#define PAREN        0x00  /* with parity */
#define PARDIS       0x10  /* no parity */
#define EVENPAR      0x00  /* even parity */
#define ODDPAR       0x04  /* odd parity */
#define CL5          0x00  /* 5 bits per char */
#define CL6          0x01  /* 6 bits per char */
#define CL7          0x02  /* 7 bits per char */
#define CL8          0x03  /* 8 bits per char */
#define PARMODEMASK  0x18  /* parity mode mask */
#define PARTYPEMASK  0x04  /* parity type mask */
#define CLMASK       0x03  /* character length mask */

/* mode register 2: MR2x operations */
#define TXRTS        0x20  /* enable transmitter RTS */
#define TXCTS        0x10  /* enable transmitter CTS */
#define SB2          0x0f  /* 2 stop bits */
#define SB1          0x07  /* 1 stop bit */
#define SB1L5        0x00  /* 1 stop bit at 5 bits per character */

#define SBMASK       0x0f  /* stop bit mask */

/* clock-select register: CSRx operations */
#define NOBAUD       -1    /* 50 and 200 baud are not possible */
/* they are not in Baud register set 2 */
#define BD75         0x00  /* 75 baud */
#define BD110        0x11  /* 110 baud */
#define BD134        0x22  /* 134.5 baud */
#define BD150        0x33  /* 150 baud */
#define BD300        0x44  /* 300 baud */
#define BD600        0x55  /* 600 baud */
#define BD1200       0x66  /* 1200 baud */
#define BD1800       0xaa  /* 1800 baud */
#define BD2400       0x88  /* 2400 baud */
#define BD4800       0x99  /* 4800 baud */
#define BD9600       0xbb  /* 9600 baud */
#define BD19200      0xcc  /* 19200 baud */

#define DEFBAUD      BD9600   /* default value if baudrate is not possible */

/* channel command register: CRx operations */
#define MRONE      	0x10  /* reset mr pointer to mr1 */
#define RXRESET      0x20  /* reset receiver */
#define TXRESET      0x30  /* reset transmitter */
#define ERRRESET     0x40  /* reset error status */
#define BRKINTRESET  0x50  /* reset channel's break interrupt */
#define BRKSTART     0x60  /* start break */
#define BRKSTOP      0x70  /* stop break */
#define MRZERO      	0xb0  /* reset mr pointer to mr0 */
#define TXDIS        0x08  /* disable transmitter */
#define TXEN         0x04  /* enable transmitter */
#define RXDIS        0x02  /* disable receiver */
#define RXEN         0x01  /* enable receiver */

/* status register: SRx status */
#define RBRK         0x80  /* received break */
#define FRERR        0x40  /* frame error */
#define PERR         0x20  /* parity error */
#define ROVRN        0x10  /* receiver overrun error */
#define TXEMT        0x08  /* transmitter empty */
#define TXRDY        0x04  /* transmitter ready */
#define FFULL        0x02  /* receiver FIFO full */
#define RXRDY        0x01  /* receiver ready */

/* auxiliary control register: ACR operations */
#define BDSET1       0x00  /* baudrate generator set 1 */
#define BDSET2       0x80  /* baudrate generator set 2 */
#define CCLK1        0x60  /* timer clock: external rate.  TA */
#define CCLK16       0x30  /* counter clock: x1 clk divided by 16 */

/* interrupt status and mask register: ISR status and IMR mask */
#define	IIPCHG		0x80	/* I/O Port Change */
#define IBRKB        0x40  /* delta break b */
#define IRXRDYB      0x20  /* receiver ready b */
#define ITXRDYB      0x10  /* transmitter ready b */
#define ITIMER       0x08  /* Counter Ready */
#define IBRKA        0x04  /* delta break a */
#define IRXRDYA      0x02  /* receiver ready a */
#define ITXRDYA      0x01  /* transmitter ready a */
