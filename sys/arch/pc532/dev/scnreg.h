/*	$NetBSD: scnreg.h,v 1.5 1994/10/26 08:24:19 cgd Exp $	*/

/* 
 * Copyright (c) 1993 Philip A. Nelson.
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
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	scnreg.h: definitions for the scn serial driver.
 */

/* Constants. */
#ifdef COMDEF_SPEED
#undef  TTYDEF_SPEED
#define TTYDEF_SPEED    COMDEF_SPEED	/* default baud rate */
#endif

#define SCN_FIRST_ADR	  0x28000000	/* address of first RS232 port */
#define SCN_FIRST_MAP_ADR 0xFFC80000	/* mapped address of first port */

#define SCN_SIZE	         0x8	/* address space for port */

#define SCN_CONSOLE 		   0	/* minor number of console */

#define SCN_CON_MAP_STAT  0xFFC80001	/* raw addresses for console */
#define SCN_CON_MAP_DATA  0xFFC80003	/* Mapped .... */
#define SCN_CON_MAP_ISR	  0xFFC80005

#define SCN_CON_STAT 	  0x28000001    /* raw addresses for console */
#define SCN_CON_DATA  	  0x28000003    /* Unmapped .... */
#define SCN_CON_ISR	  0x28000005


/* SCN2691 registers, values. */
#define LINE_SZ		0x08
#define UART_SZ		0x10
#define MR_ADR		(line_base+0)
#define STAT_ADR	(line_base+1)
#define SPEED_ADR	(line_base+1)
#define CMD_ADR		(line_base+2)
#define DATA_ADR	(line_base+3)
#define IPCR_ADR	(uart_base+4)
#define ACR_ADR		(uart_base+4)
#define ISR_ADR		(uart_base+5)
#define IMR_ADR		(uart_base+5)
#define IP_ADR		(uart_base+13)
#define OPCR_ADR	(uart_base+13)
#define SET_OP_ADR	(uart_base+14)
#define CLR_OP_ADR	(uart_base+15)

/* Data Values */
#define LC_ODD		0x04
#define	LC_EVEN		0x00
#define LC_NONE		0x10
#define LC_STOP1	0x07
#define LC_STOP2	0x0f
#define LC_BITS5	0x00
#define LC_BITS6	0x01
#define LC_BITS7	0x02
#define LC_BITS8	0x03
#define LC_CHARS	0x03
#define LC_CHARS_SHIFT  0x08
#define LC_SP_GRP	0x10
#define LC_SP_BOTH	0x20

#define RTS_BIT		0x1
#define CTS_BIT		0x1
#define DTR_BIT		0x4
#define DCD_BIT		0x4

/* CR (command) register values. */
#define CMD_ENA_RX	0x01
#define CMD_DIS_RX	0x02
#define CMD_ENA_TX	0x04
#define CMD_DIS_TX	0x08
#define CMD_MR1		0x10
#define CMD_RESET_RX	0x20
#define CMD_RESET_TX	0x30
#define CMD_RESET_ERR	0x40
#define CMD_RESET_BRK	0x50
#define CMD_START_BRK	0x60
#define CMD_STOP_BRK	0x70

/* SR register */
#define SR_RX_RDY	0x01
#define SR_TX_RDY	0x04
#define SR_BREAK	0x80
#define SR_FRAME	0x40
#define SR_PARITY	0x20
#define SR_OVERRUN	0x10

/* Output port configuration. */
#define OPCR_CONFIG	0x00

/* Input port interrupt config. */
#define ACR_CTS		0x01
#define ACR_DCD		0x04
#define IPCR_CTS	0x10
#define IPCR_DCD	0x40

/* Interrupt configurations. */
#define IMR_IP_INT	0x80
#define IMR_BRK_INT	0x04
#define IMR_RX_INT	0x02
#define IMR_TX_INT	0x01
#define IMR_BRKB_INT	0x40
#define IMR_RXB_INT	0x20
#define IMR_TXB_INT	0x10

/* If we need a delay.... */
#define DELAY(x)  {int i; for (i=0; i<x*10000; i++); }

#define istart(rs) \
  (WR_ADR(u_char, rs->opset_port, (RTS_BIT | DTR_BIT) << (rs)->a_or_b))

#define istop(rs) \
  (WR_ADR(u_char, rs->opclr_port, RTS_BIT << (rs)->a_or_b))

#define get_dcd(rs) \
  (RD_ADR(u_char, rs->ip_port) & (DCD_BIT << (1-rs->a_or_b)))

#define tx_rdy(rs) \
  (RD_ADR(u_char, rs->stat_port) & SR_TX_RDY)

/* Interrupts on and off. */
#define tx_ints_off(rs) \
    { rs->uart->imr_int_bits &=	~((IMR_TX_INT) << (4*rs->a_or_b)); \
      WR_ADR (u_char, rs->uart->imr_port, rs->uart->imr_int_bits); }

#define tx_ints_on(rs) \
    { rs->uart->imr_int_bits |= (IMR_TX_INT) << (4*rs->a_or_b); \
      WR_ADR (u_char, rs->uart->imr_port, rs->uart->imr_int_bits); }

#define rx_ints_off(rs) \
    { rs->uart->imr_int_bits &=	~((IMR_RX_INT) << (4*rs->a_or_b)); \
      WR_ADR (u_char, rs->uart->imr_port, rs->uart->imr_int_bits); }

#define rx_ints_on(rs) \
    { rs->uart->imr_int_bits |= (IMR_RX_INT) << (4*rs->a_or_b); \
      WR_ADR (u_char, rs->uart->imr_port, rs->uart->imr_int_bits); }

/* Structure definitions. */
typedef unsigned long port_t;

/* The DUART description table */
struct duart_info {
  char i_speed[2], o_speed[2];	/* Channel A and B speeds. */
  char i_code[2],  o_code[2];	/* Channel A and B speeds. */
  char speed_grp;		/* ACR bit 7 */
  char acr_int_bits;		/* ACR bits 0-6 */
  char imr_int_bits;		/* IMR bits current set. */

  port_t isr_port;
  port_t imr_port;
  port_t ipcr_port;
  port_t opcr_port;
};

/* RS232 device structure, one per device. */
struct rs232_s {
  int unit;			/* unit number of this line (base 0) */
  struct duart_info *uart;	/* pointer to uart struct */
  char a_or_b;			/* 0 => A, 1 => B */
  
  long scn_bits;		/* Temp! for TIOCM ... */

  port_t xmit_port;		/* i/o ports */
  port_t recv_port;
  port_t mr_port;
  port_t stat_port;	/* sra or srb */
  port_t speed_port;	/* csra or csrb */
  port_t cmd_port;	/* cra or crb */
  port_t acr_port;
  port_t ip_port;
  port_t opset_port;
  port_t opclr_port;

  unsigned char lstatus;	/* last line status */
  unsigned framing_errors;	/* error counts (no reporting yet) */
  unsigned overrun_errors;
  unsigned parity_errors;
  unsigned break_interrupts;

};

