/*	$NetBSD: scn.c,v 1.23 1996/01/25 19:50:39 phil Exp $ */

/*
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

#include "scn.h"

#if NSCN > 0

/* #define KERN_MORE */

/* The pc532 has 4 duarts! */
#define NLINES 8

/*
 * scn2681 driver for the pc532.  Phil Nelson  Feb 8, 1993
 * 
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>

#include <dev/cons.h>

#include <machine/icu.h>

#include "scnreg.h"

#include "sl.h"

struct scn_softc {
	struct device      scn_dev;
	struct tty        *scn_tty;
	struct rs232_s     scn_line;
	char 		   scn_swflags;
#define SCN_SW_SOFTCAR 0x01
#define SCN_SW_CLOCAL  0x02
#define SCN_SW_CRTSCTS 0x04
};

int 	scnprobe __P((struct device *, void *, void *));
void    scnattach __P((struct device *, struct device *, void *));
int     scnintr __P((int));
int	scnparam __P((struct tty *, struct termios *));
void	scnstart __P((struct tty *));
int	scnopen __P((dev_t, int, int, struct proc *));
int	scnclose __P((dev_t, int, int, struct proc *));

struct cfdriver scncd =
      {	NULL, "scn", scnprobe, scnattach,
	DV_TTY, sizeof(struct scn_softc), NULL, 0 };

/* int	scnsoftCAR;
int	scn_active;  To Be Deleted ... */
int	scnconsole = SCN_CONSOLE;
int	scnconsinit = 0;
int	scndefaultrate = TTYDEF_SPEED;
int	scnmajor;

struct duart_info  uart[(NLINES+1)/2];
/* struct rs232_s line[NLINES];
   struct tty *scn_tty[NLINES];  To be deleted. */

struct speedtab scnspeedtab[] = {
	0,	0x40,	/* code for line-hangup */
	50,	0x00,
	75,	0x10,
	110,	0x21,
	134,	0x22,
	150,	0x13,
	200,	0x03,
	300,	0x24,
	600,	0x25,
	1050,	0x07,
	1200,	0x26,
	1800,	0x1a,
	2000,	0x17,
	2400,	0x28,
	4800,	0x29,
	7200,   0x0a,
	9600,	0x2b,
	19200,	0x1c,
	38400,	0x0c,
	57600,	0x60,	/* An illegal speed....? */
	-1,	-1
};

#define getspeedcode(sp,val) \
	{ int i=0; \
	  while (scnspeedtab[i].sp_speed != -1 && \
		 scnspeedtab[i].sp_speed != sp) \
	    i++; \
	  val = scnspeedtab[i].sp_code; \
	}

/* Unit is 0-7. Other parts of the minor number are things like
   hardware cts/rts handshaking.  (XXX how?) */

#define UNIT(x)	(minor(x) & 0x7)

/* Which uart is the device on? */
#define UART(x) (UNIT(x) >> 1)

extern	struct tty *constty;

#ifdef KGDB
#include "machine/remote-sl.h"

extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif


/* Debug routine to print out the rs line structures. */
void print_rs (struct rs232_s *rs)
{
  printf ("\nline frame overrun parity break\n");
  printf ("tty%1d state=%2x  f=%2d o=%2d p=%2d b=%2d in=%x, out=%x grp=%d\n",
             rs->unit, rs->framing_errors, rs->overrun_errors,
	     rs->parity_errors, rs->break_interrupts, 
	     rs->uart->i_speed[rs->a_or_b], rs->uart->o_speed[rs->a_or_b],
	     rs->uart->speed_grp);
}


/*==========================================================================*
 *				scn_config			 	    *
 *==========================================================================*/
static
int scn_config(unit, in_speed, out_speed, parity, stop_bits, data_bits )
int unit;			/* which rs line */
int in_speed;			/* input speed: 110, 300, 1200, etc */
int out_speed;			/* output speed: 110, 300, 1200, etc */
int parity;			/* some parity */
int stop_bits;			/* 2 (110 baud) or 1 (other speeds) */
int data_bits;			/* 5, 6, 7, or 8 */
{
/* Set various line control parameters for RS232 I/O. */

  register struct rs232_s *rs;
  char mr1_val, mr2_val;
  int  sp_grp, sp_both;
  char set_speed;            /* Non zero if we need to set the speed. */
  char a_or_b;               /* Used for ease of access. */
  int in_code;
  int out_code;
  int x;

  /* Get the speed codes. */
  getspeedcode(in_speed,in_code);
  getspeedcode(out_speed,out_code);

  /* Check for speed errors. */
  if (in_code == -1 || out_code == -1) 
    return (EINVAL);

  /* Set up rs pointer. */
  if (unit >= scncd.cd_ndevs)
  	return  ENXIO;
  rs = &((struct scn_softc *)scncd.cd_devs[unit])->scn_line;
  a_or_b = rs->a_or_b;

  /* Check out the Speeds. There are two groups of speeds.  If the new
     speeds are not in the same group, or the other line is not the same
     speed of the other group, do not change the speeds.  Also, if the
     in speed and the out speed are in different groups, use the in speed. */

  set_speed = FALSE;
  if ( (in_code != rs->uart->i_code[a_or_b])
      || (out_code != rs->uart->o_code[a_or_b])) {

    /* We need to set the speeds .*/
    set_speed = TRUE;

    if ( ((in_code & LC_SP_GRP) != (out_code & LC_SP_GRP))
       && (((in_code | out_code) & LC_SP_BOTH) != 1) ) {
      /* Input speed and output speed are different groups. */
      return (EINVAL);
    }

    sp_grp = ((in_code | out_code) & LC_SP_GRP)>>4;
    sp_both = in_code & out_code & LC_SP_BOTH;

    /* Check for compatibility and set the uart values */
    if (sp_both)
      sp_grp = rs->uart->speed_grp;
    else
      if ((sp_grp != rs->uart->speed_grp)
	  && !(rs->uart->i_code[1-a_or_b] & rs->uart->o_code[1-a_or_b] 
	       & LC_SP_BOTH)) {
	/* Can't change group, don`t change the speed rates. */
	return (EINVAL);
      }
    rs->uart->i_code[a_or_b] = in_code;
    rs->uart->o_code[a_or_b] = out_code;
    rs->uart->i_speed[a_or_b] = in_speed;
    rs->uart->o_speed[a_or_b] = out_speed;
  }

  /* Lock out interrupts while setting the parameters.  (Just for safety.)
   */
  x=spltty();
  WR_ADR (u_char, rs->cmd_port, CMD_MR1);
  mr1_val = RD_ADR (u_char, rs->mr_port);
  mr2_val = RD_ADR (u_char, rs->mr_port);
  if ((((mr1_val & 0xe0) | parity | data_bits) != mr1_val) ||
      (((mr2_val & 0xf0) | stop_bits) != mr2_val)) {
    WR_ADR (u_char, rs->cmd_port, CMD_MR1);
    WR_ADR (u_char, rs->mr_port, (mr1_val & 0xe0) | parity | data_bits);
    WR_ADR (u_char, rs->mr_port, (mr2_val & 0xf0) | stop_bits);
  }
  if (set_speed) {
    if (rs->uart->speed_grp != sp_grp) {
	/* Change the group! */
	rs->uart->speed_grp = sp_grp;
	WR_ADR (u_char, rs->acr_port, (sp_grp << 7) | rs->uart->acr_int_bits);
    }
    WR_ADR (u_char, rs->speed_port, ((in_code & 0x0f) << 4) 
    					| (out_code & 0x0f));
  }
  DELAY(96000/out_speed);

  splx(x);
  return (0);
}


scnprobe(parent, cf, aux)
	struct device	*parent;
	void		*cf;
	void		*aux;
{
  int unit = ((struct cfdata *)cf)->cf_unit;

  if (unit >= NLINES) {
    return(0);  /* dev is "not working." */
  } else {
    return(1);  /* dev is "working." */
  }
}

void
scnattach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
  static char scnints[4] = { IR_TTY0, IR_TTY1, IR_TTY2, IR_TTY3 };
  struct  scn_softc *sc = (void *) self;
  struct  tty  *tp;
  u_char  unit = sc->scn_dev.dv_unit;
  u_char  duart = unit >> 1;
  register struct rs232_s *rs = &sc->scn_line;
  int	x;
  int speed;
  long line_base;
  long uart_base;
  long scn_first_adr;

  if (unit == 0)  DELAY(5);  /* Let the output go out.... */

  sc->scn_swflags |= SCN_SW_SOFTCAR;
 
  /* Record unit number, uart, channel a_or_b. */
  rs->unit = unit;
  rs->uart = &uart[unit >> 1];
  rs->a_or_b = unit % 2;

  /* Establish interrupt vector */
  if (rs->a_or_b == 0) {
    /* Arg 0 is special, so we must pass "unit + 1" */
    intr_establish(scnints[unit >> 1], (void (*)(void *))scnintr,
	(void *)(unit + 1), sc->scn_dev.dv_xname, IPL_TTY, FALLING_EDGE);
  }

  /* Precalculate port numbers for speed. Magic numbers in the code (once). */
  scn_first_adr = SCN_FIRST_MAP_ADR;  /* to get around a gcc bug. */
  line_base = scn_first_adr + LINE_SZ * unit;
  uart_base = scn_first_adr + UART_SZ * duart;
  rs->xmit_port = DATA_ADR;
  rs->recv_port = DATA_ADR;
  rs->mr_port = MR_ADR;
  rs->stat_port = STAT_ADR;
  rs->speed_port = SPEED_ADR;
  rs->cmd_port = CMD_ADR;
  rs->acr_port = ACR_ADR;
  rs->ip_port = IP_ADR;
  rs->opset_port = SET_OP_ADR;
  rs->opclr_port = CLR_OP_ADR;

  /* Initialize error counts */
  rs->framing_errors = 0;
  rs->overrun_errors = 0;
  rs->parity_errors = 0;
  rs->break_interrupts = 0;
  
  /* Set up the hardware to a base state, in particular
   *   o reset transmitter and receiver
   *   o set speeds and configurations 
   *   o receiver interrupts only (RxRDY and BREAK)
   */

  x = spltty();
  istop(rs);			/* CTS off... */

  WR_ADR (u_char, rs->cmd_port, CMD_DIS_RX | CMD_DIS_TX);
  WR_ADR (u_char, rs->cmd_port, CMD_RESET_RX);
  WR_ADR (u_char, rs->cmd_port, CMD_RESET_TX);
  WR_ADR (u_char, rs->cmd_port, CMD_RESET_ERR);
  WR_ADR (u_char, rs->cmd_port, CMD_RESET_BRK);
  WR_ADR (u_char, rs->cmd_port, CMD_MR1);
  WR_ADR (u_char, rs->mr_port, 0);	/* No receiver control of RTS. */
  WR_ADR (u_char, rs->mr_port, 0);
#if 1
  if (unit != 0) {
    WR_ADR (u_char, rs->mr_port, 0x80);  /* Enable receiver control of RTS */
    WR_ADR (u_char, rs->mr_port, 0x10);  /* Enable CTS transmitter control */
  }
#endif

  /* Initialize the uart structure if this is channel A. */
  if (rs->a_or_b == 0) {
    /* uart ports */
    rs->uart->isr_port = ISR_ADR;
    rs->uart->imr_port = IMR_ADR;
    rs->uart->ipcr_port = IPCR_ADR;
    rs->uart->opcr_port = OPCR_ADR;

    /* Disable all interrupts. */
    WR_ADR (u_char, rs->uart->imr_port, 0);
    rs->uart->imr_int_bits = 0;

    /* Output port config */
    WR_ADR (u_char, rs->uart->opcr_port, 0);

    /* Speeds... */
    rs->uart->speed_grp = 1;
    rs->uart->acr_int_bits = 0;
    /* Set initial speed to an illegal code that can be changed to
       any other baud. */
    rs->uart->i_code[0] = rs->uart->o_code[0] =
      rs->uart->i_code[1] = rs->uart->o_code[1] = 0x2f;
    rs->uart->i_speed[0] = rs->uart->o_speed[0] =
      rs->uart->i_speed[1] = rs->uart->o_speed[1] = 0x0;
  }

#if 0
  rs->uart->acr_int_bits |= ACR_CTS << rs->a_or_b;	/* Set CTS int */
#endif
  WR_ADR (u_char, rs->acr_port,
  	 (rs->uart->speed_grp << 7) | rs->uart->acr_int_bits);
#ifdef CONSOLE_SPEED
  if (unit == 0)
{ printf ("scn_config on unit 0\n");    
    scn_config(unit, CONSOLE_SPEED, CONSOLE_SPEED,
	       LC_NONE, LC_STOP1, LC_BITS8);
}
  else
#endif
    scn_config(unit, scndefaultrate, scndefaultrate,
	       LC_NONE, LC_STOP1, LC_BITS8);

  /* Turn on the Rx and Tx. */
  WR_ADR (u_char, rs->cmd_port, CMD_ENA_RX | CMD_ENA_TX);

  /* Set up the interrupts. */
  rs->uart->imr_int_bits
     |= (/*IMR_RX_INT | IMR_TX_INT |*/ unit?IMR_BRK_INT:0) << (4*rs->a_or_b) | IMR_IP_INT;
  WR_ADR (u_char, rs->uart->imr_port, rs->uart->imr_int_bits);

  splx(x);

#ifdef KGDB
	if (kgdb_dev == makedev(scnmajor, unit+1)) {
		if (scnconsole == unit)
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			(void) scninit(unit, kgdb_rate);
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("scn%d: ", unit);
				kgdb_connect(1);
			} else
				printf("scn%d: kgdb enabled\n", unit);
		}
	}
#endif

  /* print the device number... */
  printf (" addr 0x%x\n", line_base);
}

/* ARGSUSED */
scnopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct  scn_softc *sc;
	register struct tty *tp;
	register int unit = UNIT(dev);
	register struct rs232_s *rs; 
	int error = 0;
	int x;

	/* Set up rs pointer. */
	if (unit >= scncd.cd_ndevs)
		return ENXIO;
	sc = scncd.cd_devs[unit];
	if (!sc)
		return ENXIO;
	rs = &sc->scn_line;

	x = spltty();

	if(!sc->scn_tty) {
		tp = sc->scn_tty = ttymalloc();
	} else
		tp = sc->scn_tty;

	tp->t_oproc = scnstart;
	tp->t_param = scnparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		if (sc->scn_swflags & SCN_SW_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (sc->scn_swflags & SCN_SW_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = scndefaultrate;
		scnparam(tp, &tp->t_termios);
		ttsetwater(tp);

		/* Turn on DTR and RTS. */
		istart(rs);
		rx_ints_on (rs);

		/* Carrier?  XXX fix more like i386 */
		if ((sc->scn_swflags & SCN_SW_SOFTCAR) || get_dcd(rs))
			tp->t_state |= TS_CARR_ON; 
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(x);
		return (EBUSY);
	}

	/* wait for carrier if necessary */
	if ((flag & O_NONBLOCK) == 0)
		while ((tp->t_cflag & CLOCAL) == 0 &&
		    (tp->t_state & TS_CARR_ON) == 0) {
			tp->t_state |= TS_WOPEN;
			error = ttysleep(tp, (caddr_t)&tp->t_rawq, 
			    TTIPRI | PCATCH, ttopen, 0);
			if (error) {
				/* XXX should turn off chip if we're the
				   only waiter */
				splx(x);
				return error;
			}
		}
	splx(x);
		
	return (*linesw[tp->t_line].l_open)(dev, tp);
}
 
/*ARGSUSED*/
scnclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register int unit = UNIT(dev);
	struct   scn_softc *sc = scncd.cd_devs[unit];
	register struct tty *tp = sc->scn_tty;
	register struct rs232_s *rs = &sc->scn_line;

	(*linesw[tp->t_line].l_close)(tp, flag);
#ifdef KGDB
	/* do not disable interrupts if debugging */
	if (kgdb_dev != makedev(scnmajor, unit))
#endif
 	if ((tp->t_state&TS_ISOPEN) == 0)
		rx_ints_off (rs);
	if (tp->t_cflag&HUPCL || tp->t_state&TS_WOPEN || 
	    (tp->t_state&TS_ISOPEN) == 0) {
		WR_ADR (u_char, rs->opclr_port, DTR_BIT << rs->a_or_b);
		DELAY (10);
		WR_ADR (u_char, rs->opset_port, DTR_BIT << rs->a_or_b);
	}
	ttyclose(tp);
#if 0
	if ((tp->t_state&TS_ISOPEN) == 0) {
		ttyfree(tp);
		sc->scn_tty = (struct tty *)NULL;
	}
#endif
	return(0);
}
 
scnread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	register struct scn_softc *sc = scncd.cd_devs[UNIT(dev)];
	register struct tty *tp = sc->scn_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
scnwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	register struct scn_softc *sc = scncd.cd_devs[UNIT(dev)];
	register struct tty *tp = sc->scn_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
scntty(dev)
	dev_t dev;
{
	register struct scn_softc *sc = scncd.cd_devs[UNIT(dev)];
	register struct tty *tp = sc->scn_tty;

	return (tp);
}

void cts_int (struct rs232_s *rs, struct tty *tp)
{
}

#if 0 
scnintr(int uart_no)
{
  int line0 = uart_no << 1;
  int line1 = (uart_no << 1)+1;

  register struct scn_softc *sc0 = scncd.cd_devs[line0];
  register struct scn_softc *sc1 = scncd.cd_devs[line1];

  register struct tty *tp0 = sc0->scn_tty;
  register struct tty *tp1 = sc1->scn_tty;

  register struct rs232_s *rs0 = &sc0->scn_line;
  register struct rs232_s *rs1 = &sc1->scn_line;

  register struct duart_info *uart = rs0->uart;

  char   rs_work = TRUE;

  u_char   rs_stat;
  u_char   rs_ipcr;
  u_char   ch;
  

  rs_stat = RD_ADR(u_char, uart->isr_port);
  printf ("scnintr, rs_stat = 0x%x\n", rs_stat);

	if (rs_stat & IMR_BRK_INT) {
		/* A break interrupt! */
		rs0->lstatus = RD_ADR(u_char, rs0->stat_port);
		printf ("lstatus = 0x%x\n", rs0->lstatus);
		if (rs0->lstatus & SR_BREAK) {
		  	++rs0->break_interrupts;
			RD_ADR(u_char, rs0->recv_port);	/* Toss zero character. */
			rs_stat &= ~IMR_RX_INT;
		}
		WR_ADR (u_char, rs0->cmd_port, CMD_RESET_BRK);
		rs0->lstatus = RD_ADR(u_char, rs0->stat_port);
		printf ("lstatus = 0x%x\n", rs0->lstatus);
	}
	if (rs_stat & IMR_RX_INT) {
		ch = RD_ADR(u_char, rs0->recv_port);
		printf ("input ch = \"%c\"\n", ch);
	}
}
/* Change this for real interrupts! */
_scnintr(int uart_no)

#else

/* Change this for real interrupts! */
scnintr(int line1)
#endif
{
  register struct scn_softc *sc0 = scncd.cd_devs[line1 - 1];
  register struct scn_softc *sc1 = scncd.cd_devs[line1];

  register struct tty *tp0 = sc0->scn_tty;
  register struct tty *tp1 = sc1->scn_tty;

  register struct rs232_s *rs0 = &sc0->scn_line;
  register struct rs232_s *rs1 = &sc1->scn_line;

  register struct duart_info *uart = rs0->uart;

  char   rs_work = TRUE;

  u_char   rs_stat;
  u_char   rs_ipcr;
  u_char   ch;

#ifdef CON_BRK_PANIC
  u_char   nr_brk = 0;
#endif

  while (rs_work) {
	/* Loop to pick up ALL pending interrupts for device.
	 */
	rs_work = FALSE;
	rs_stat = RD_ADR(u_char, uart->isr_port);
/* if (rs_stat & ~(IMR_TX_INT | IMR_TXB_INT)) printf ("scn intr rs_stat = 0x%x\n", rs_stat); */
	if ((rs_stat & IMR_TX_INT) && (tp0 != NULL) 
	    && (tp0->t_state & TS_BUSY)) {
		/* output char done. */
		tp0->t_state &= ~(TS_BUSY|TS_FLUSH);
		tx_ints_off(rs0);
		if (tp0->t_line)
			(*linesw[tp0->t_line].l_start)(tp0);
		else
			scnstart(tp0);
	 	rs_work = TRUE;
	}
	if (rs_stat & IMR_BRK_INT && (tp0 != NULL)) {
		/* A break interrupt! */
		rs0->lstatus = RD_ADR(u_char, rs0->stat_port);
		if (rs0->lstatus & SR_BREAK) {
		  	++rs0->break_interrupts;
			RD_ADR(u_char, rs0->recv_port);	/* Toss zero character. */
			rs_stat &= ~IMR_RX_INT;
		}
		WR_ADR (u_char, rs0->cmd_port, CMD_RESET_BRK);
		rs_work = TRUE;
#ifdef CON_BRK_PANIC
		if (line1 == 1 && (rs0->lstatus & SR_BREAK)) {
			if (++nr_brk >= 3) {
				char c;
				printf("\r\nDo you want a dump (y/n)? ");
				do
					c = cngetc();
				while (c != 'y' && c != 'n');
				printf("%c\r\n", c);
				if (c == 'y') {
					panic("Panic Button");
				}
			}
		}
#endif
	}
	if (rs_stat & IMR_RX_INT && (tp0 != NULL)) {
		ch = RD_ADR(u_char, rs0->recv_port);
		if (tp0->t_state & TS_ISOPEN) 
			(*linesw[tp0->t_line].l_rint)(ch, tp0);
		rs_work = TRUE;
#ifdef CON_BRK_PANIC
		if (line1 == 1) nr_brk = 0;
#endif
	}
	if ((rs_stat & IMR_TXB_INT)  && (tp1 != NULL)
	     && (tp1->t_state & TS_BUSY)) {
		/* output char done. */
		tp1->t_state &= ~(TS_BUSY|TS_FLUSH);
		tx_ints_off(rs1);
		if (tp1->t_line)
			(*linesw[tp1->t_line].l_start)(tp1);
		else
			scnstart(tp1);
	 	rs_work = TRUE;
	}
	if (rs_stat & IMR_BRKB_INT && (tp1 != NULL)) {
		/* A break interrupt! */
		rs1->lstatus = RD_ADR(u_char, rs1->stat_port);
		if (rs1->lstatus & SR_BREAK) {
		  	++rs1->break_interrupts;
			RD_ADR(u_char, rs1->recv_port);	/* Toss zero character. */
			rs_stat &= ~IMR_RXB_INT;
		}
		WR_ADR (u_char, rs1->cmd_port, CMD_RESET_BRK);
		rs_work = TRUE;
	}
	if (rs_stat & IMR_RXB_INT && (tp1 != NULL)) {
		ch = RD_ADR(u_char, rs1->recv_port);
		if (tp1->t_state & TS_ISOPEN) 
			(*linesw[tp1->t_line].l_rint)(ch, tp1);
		rs_work = TRUE;
	}
	if (rs_stat & IMR_IP_INT) {
		rs_work = TRUE;
		rs_ipcr = RD_ADR (u_char, uart->ipcr_port);
#if 0
	/* RTS/CTS stuff! */
		if (rs_ipcr & IPCR_CTS)
			cts_int(rs0, tp0);
		if (rs_ipcr & (IPCR_CTS << 1))
			cts_int(rs1, tp1);
#endif
#if 0
		if (rs_ipcr & ACR_DCD)
			dcd_int(rs0, tp0);
		if (rs_ipcr & (ACR_DCD << 1))
			dcd_int(rs1, tp1);
#endif
	}
  }
}


scnioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register int unit = UNIT(dev);
	register struct scn_softc *sc = scncd.cd_devs[unit];
	register struct tty *tp = sc->scn_tty;
	register struct rs232_s *rs = &sc->scn_line;
	register scn;
	register int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		WR_ADR(u_char, rs->cmd_port, CMD_START_BRK);
		break;

	case TIOCCBRK:
		WR_ADR (u_char, rs->cmd_port, CMD_STOP_BRK);
		break;

	case TIOCSDTR:
		WR_ADR(u_char, rs->opset_port,
			 (DTR_BIT | RTS_BIT) << rs->a_or_b);
		break;

	case TIOCCDTR:
		WR_ADR(u_char, rs->opclr_port, 
			 (DTR_BIT | RTS_BIT) << rs->a_or_b);
		break;

	case TIOCMSET:
/*		(void) scnmctl(dev, *(int *)data, DMSET); */
		rs->scn_bits = *(long *)data;
		break;

	case TIOCMBIS:
/*		(void) scnmctl(dev, *(int *)data, DMBIS); */
		rs->scn_bits |= *(int *)data;
		break;

	case TIOCMBIC:
/*		(void) scnmctl(dev, *(int *)data, DMBIC); */
		rs->scn_bits &= ~(*(int *)data);
		break;

	case TIOCMGET:
/*		*(int *)data = scnmctl(dev, 0, DMGET); */
		*(int *)data = rs->scn_bits;
		break;

/* 386	case TIOCGFLAGS: {
		int bits = 0;

		if (sc->sc_swflags & COM_SW_SOFTCAR)
			bits |= TIOCFLAG_SOFTCAR;
		if (sc->sc_swflags & COM_SW_CLOCAL)
			bits |= TIOCFLAG_CLOCAL;
		if (sc->sc_swflags & COM_SW_CRTSCTS)
			bits |= TIOCFLAG_CRTSCTS;
		if (sc->sc_swflags & COM_SW_MDMBUF)
			bits |= TIOCFLAG_MDMBUF;

		*(int *)data = bits;
		break;
	}
	case TIOCSFLAGS: {
		int userbits, driverbits = 0;

		error = suser(p->p_ucred, &p->p_acflag); 
		if (error != 0)
			return(EPERM); 

		userbits = *(int *)data;
		if ((userbits & TIOCFLAG_SOFTCAR) ||
		    (sc->sc_hwflags & COM_HW_CONSOLE))
			driverbits |= COM_SW_SOFTCAR;
		if (userbits & TIOCFLAG_CLOCAL)
			driverbits |= COM_SW_CLOCAL;
		if (userbits & TIOCFLAG_CRTSCTS)
			driverbits |= COM_SW_CRTSCTS;
		if (userbits & TIOCFLAG_MDMBUF)
			driverbits |= COM_SW_MDMBUF;

		sc->sc_swflags = driverbits;
		break;
	}
*/
	default:
		return (ENOTTY);
	}
	return (0);
}

scnparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
  int cflag = t->c_cflag;
  int unit = UNIT(tp->t_dev);
  register struct scn_softc *sc = scncd.cd_devs[unit];
  int parity = LC_NONE,
      stop_bits = LC_STOP1,
      data_bits = LC_BITS8;
  int error;
  struct rs232_s *rs = &sc->scn_line;

  /* Is this a hang up? */
  if (t->c_ospeed == B0) {
	WR_ADR (u_char, rs->opclr_port, DTR_BIT << rs->a_or_b);
	DELAY (10);
	WR_ADR (u_char, rs->opset_port, DTR_BIT << rs->a_or_b);
	return(0);
  }

  /* Parity? */
  if (cflag&PARENB) {
	if ((cflag&PARODD) == 0)
		parity = LC_EVEN;
	else
		parity = LC_ODD;
  }

 /* Stop bits. */
 if (cflag&CSTOPB)
	stop_bits = LC_STOP1;

  /* Data bits. */
  switch (cflag&CSIZE) {
  case CS5:
	data_bits = LC_BITS5; break;
  case CS6:
	data_bits = LC_BITS6; break;
  case CS7:
	data_bits = LC_BITS7; break;
  case CS8:
	data_bits = LC_BITS8; break;
  }

  error = scn_config (unit, t->c_ispeed, t->c_ospeed, parity, stop_bits,
  			 data_bits);

  /* If successful, copy to tty */
  if (!error) {
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;
  }

  return (error);
}

void 
scnstart(tp)
	register struct tty *tp;
{
	int s, c;
	int unit = UNIT(tp->t_dev);
	register struct scn_softc *sc = scncd.cd_devs[unit];
	struct rs232_s *rs = &sc->scn_line;
 
	s = spltty();
	if (tp->t_state & (TS_BUSY|TS_TTSTOP))
		goto out;
 	if (tp->t_outq.c_cc <= tp->t_lowat) {
 		if (tp->t_state&TS_ASLEEP) {
 			tp->t_state &= ~TS_ASLEEP;
 			wakeup((caddr_t)&tp->t_outq);
 		}
 		selwakeup(&tp->t_wsel);
 	}
 	if (tp->t_outq.c_cc == 0)
 		goto out;
	tp->t_state |= TS_BUSY;
	if (tx_rdy(rs)) {
		c = getc(&tp->t_outq);
		WR_ADR(u_char, rs->xmit_port, c);
		tx_ints_on(rs);
	}
out:
	splx(s);
}
 
/*
 * Stop output on a line.
 */
/*ARGSUSED*/
scnstop(tp, flag)
	register struct tty *tp;
{
	register int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state&TS_TTSTOP)==0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}
 
/*
 * Following are all routines needed for SCN to act as console
 */

scncnprobe(cp)
	struct consdev *cp;
{
	/* locate the major number */
	for (scnmajor = 0; scnmajor < nchrdev; scnmajor++)
		if (cdevsw[scnmajor].d_open == scnopen)
			break;

	/* make sure hardware exists?  XXX */

	/* initialize required fields */
	cp->cn_dev = makedev(scnmajor, SCN_CONSOLE);
	cp->cn_pri = CN_NORMAL;
	return 1;
}

scncninit(cp)
	struct consdev *cp;
{
#if 0
	int unit = UNIT(cp->cn_dev);

	scninit(unit, scndefaultrate);
	scnconsole = unit;
	scnconsinit = 1;
#endif
}

scninit(unit, rate)
	int unit, rate;
{
#if 0
	register int scn;
	int s;
	short stat;

#ifdef lint
	stat = unit; if (stat) return;
#endif
	scn = scn_addr[unit];
	s = splhigh();
	outb(com+com_cfcr, CFCR_DLAB);
	rate = ttspeedtab(comdefaultrate, comspeedtab);
	outb(com+com_data, rate & 0xFF);
	outb(com+com_ier, rate >> 8);
	outb(com+com_cfcr, CFCR_8BITS);
	outb(com+com_ier, IER_ERXRDY | IER_ETXRDY);
	outb(com+com_fifo, FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_14);
	stat = inb(com+com_iir);
	splx(s);
#endif
}

#if 0
int
scnselect(dev, rw, p)
 	dev_t dev;
 	int rw;
 	struct proc *p;
{
 	register struct tty *tp = scn_tty[UNIT(dev)];
 	int nread;
 	int s = spltty();
        struct proc *selp;
 
 	switch (rw) {
 
 	case FREAD:
 		nread = ttnread(tp);
 		if (nread > 0 || 
 		   ((tp->t_cflag&CLOCAL) == 0 && (tp->t_state&TS_CARR_ON) == 0))
 			goto win;
 		selrecord(p, &tp->t_rsel);
 		break;
 
 	case FWRITE:
 		if (tp->t_outq.c_cc <= tp->t_lowat)
 			goto win;
 		selrecord(p, &tp->t_wsel);
 		break;
 	}
 	splx(s);
 	return (0);
  win:
 	splx(s);
 	return (1);
}
#endif


/* So the kernel can write in unmapped mode! */
int _mapped = 0;

/*
 * Console kernel input character routine.
 */

char
scncngetc(dev_t dev)
{
   char c;
   int x = spltty();
   WR_ADR (u_char, SCN_FIRST_MAP_ADR + 14, (RTS_BIT | DTR_BIT));
   while (0 == (RD_ADR (u_char, SCN_CON_MAP_STAT) & SR_RX_RDY));
   c = RD_ADR(u_char, SCN_CON_MAP_DATA);
   splx(x);
   return c;
}

/* pc532 does not turn off console polling. */
char
scncnpollc(dev_t dev, int on)
{ 
}


/*
 * Console kernel output character routine.
 */

/* A simple kernel level "more" for debugging output. */
#ifdef KERN_MORE
int ___lines = 0;
#endif

scncnputc (dev_t dev, char c)
{
  int x = spltty();

  if (c == '\n') scncnputc(dev,'\r');
  if (_mapped) {
    while (0 == (RD_ADR (u_char, SCN_CON_MAP_STAT) & SR_TX_RDY));
    WR_ADR (u_char, SCN_CON_MAP_DATA, c);
    while (0 == (RD_ADR (u_char, SCN_CON_MAP_STAT) & SR_TX_RDY));
    RD_ADR(u_char, SCN_CON_MAP_ISR);  
  } else {
    while (0 == (RD_ADR (u_char, SCN_CON_STAT) & SR_TX_RDY));
    WR_ADR (u_char, SCN_CON_DATA, c);
    while (0 == (RD_ADR (u_char, SCN_CON_STAT) & SR_TX_RDY));
    RD_ADR(u_char, SCN_CON_ISR);  
  }
#ifdef KERN_MORE
  if (c == '\n' && ___lines >= 0)
    {
     if (++___lines == 22) {
	___lines = 0;
	scncnputc(dev,'m');scncnputc(dev,'o');scncnputc(dev,'r');
	scncnputc(dev,'e');scncnputc(dev,':');scncnputc(dev,' ');
	scncngetc(dev);
	scncnputc(dev,'\n');
     }
  }
#endif
  splx(x);
}

#endif
