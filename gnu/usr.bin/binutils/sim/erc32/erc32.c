/*
 * This file is part of SIS.
 * 
 * SIS, SPARC instruction simulator V1.8 Copyright (C) 1995 Jiri Gaisler,
 * European Space Agency
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

/* The control space devices */

#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include "sis.h"
#include "end.h"

extern int32    sis_verbose;
extern int      mecrev0;
extern char     uart_dev1[], uart_dev2[];

#define MEC_WS	0		/* Waitstates per MEC access (0 ws) */
#define MOK	0

/* MEC register addresses */

#define MEC_UARTA	0x0E0
#define MEC_UARTB	0x0E4
#define MEC_UART_CTRL	0x0E8
#define MEC_TIMER_CTRL	0x098
#define MEC_RTC_COUNTER	0x080
#define MEC_RTC_RELOAD	0x080
#define MEC_RTC_SCALER	0x084
#define MEC_GPT_COUNTER	0x088
#define MEC_GPT_RELOAD	0x088
#define MEC_GPT_SCALER	0x08C
#define MEC_DBG		0x0C0
#define MEC_BRK		0x0C4
#define MEC_WPR		0x0C8
#define MEC_SFSR	0x0A0
#define MEC_FFAR	0x0A4
#define MEC_IPR		0x048
#define MEC_IMR 	0x04C
#define MEC_ICR 	0x050
#define MEC_IFR 	0x054
#define MEC_MCR		0x000
#define MEC_MEMCFG	0x010
#define MEC_WCR		0x018
#define MEC_MAR0  	0x020
#define MEC_MAR1  	0x024
#define MEC_SFR  	0x004
#define MEC_WDOG  	0x060
#define MEC_TRAPD  	0x064
#define MEC_PWDR  	0x008
#define SIM_LOAD	0x0F0

/* Memory exception causes */
#define PROT_EXC	0x3
#define UIMP_ACC	0x4
#define MEC_ACC		0x6
#define WATCH_EXC	0xa
#define BREAK_EXC	0xb

/* Size of UART buffers (bytes) */
#define UARTBUF	1024

/* Number of simulator ticks between flushing the UARTS. 	 */
/* For good performance, keep above 1000			 */
#define UART_FLUSH_TIME	  3000

/* MEC timer control register bits */
#define TCR_GACR 1
#define TCR_GACL 2
#define TCR_GASE 4
#define TCR_GASL 8
#define TCR_TCRCR 0x100
#define TCR_TCRCL 0x200
#define TCR_TCRSE 0x400
#define TCR_TCRSL 0x800

/* New uart defines */
#define UART_TX_TIME	1000
#define UART_RX_TIME	1000
#define UARTA_DR	0x1
#define UARTA_SRE	0x2
#define UARTA_HRE	0x4
#define UARTA_OR	0x40
#define UARTA_CLR	0x80
#define UARTB_DR	0x10000
#define UARTB_SRE	0x20000
#define UARTB_HRE	0x40000
#define UARTB_OR	0x400000
#define UARTB_CLR	0x800000

#define UART_DR		0x100
#define UART_TSE	0x200
#define UART_THE	0x400

/* MEC registers */

static char     fname[256];
static uint32   find = 0;
static char     simfn[] = "simload";
static uint32   brk_point = 0;
static uint32   watch_point = 0;
static uint32   mec_dbg = 0;
static uint32   mec_sfsr = 0x078;
static uint32   mec_ffar = 0;
static uint32   mec_ipr = 0;
static uint32   mec_imr = 0x3fff;
static uint32   mec_icr = 0;
static uint32   mec_ifr = 0;
static uint32   mec_mcr;	/* MEC control register */
static uint32   mec_memcfg;	/* Memory control register */
static uint32   mec_wcr;	/* MEC waitstate register */
static uint32   mec_mar0;	/* MEC access registers (2) */
static uint32   mec_mar1;	/* MEC access registers (2) */
static uint32   mec_regs[64];
static uint32   posted_irq;
static uint32   mec_ersr = 0;	/* MEC error and status register */
static uint32   mec_emr = 0x60;	/* MEC error mask register */
static uint32   mec_tcr = 0;	/* MEC test comtrol register */

static uint32   rtc_counter = 0xffffffff;
static uint32   rtc_reload = 0xffffffff;
static uint32   rtc_scaler = 0xff;
static uint32   rtc_enabled = 0;
static uint32   rtc_cr = 0;
static uint32   rtc_se = 0;
static uint32   rtc_cont = 0;

static uint32   gpt_counter = 0xffffffff;
static uint32   gpt_reload = 0xffffffff;
static uint32   gpt_scaler = 0xffff;
static uint32   gpt_enabled = 0;
static uint32   gpt_cr = 0;
static uint32   gpt_se = 0;
static uint32   gpt_cont = 0;

static uint32   wdog_scaler;
static uint32   wdog_counter;
static uint32   wdog_rst_delay;
static uint32   wdog_rston;

#ifdef MECREV0
static uint32   gpt_irqon = 1;
static uint32   rtc_irqon = 1;
#endif

enum wdog_type {
    init, disabled, enabled, stopped
};

static enum wdog_type wdog_status;

/* Memory support variables */

static uint32   mem_ramr_ws;	/* RAM read waitstates */
static uint32   mem_ramw_ws;	/* RAM write waitstates */
static uint32   mem_romr_ws;	/* ROM read waitstates */
static uint32   mem_romw_ws;	/* ROM write waitstates */
static uint32   mem_ramsz;	/* RAM size */
static uint32   mem_romsz;	/* RAM size */
static uint32   mem_banksz;	/* RAM bank size */
static uint32   mem_accprot;	/* RAM write protection enabled */

/* UART support variables */

static unsigned char Adata, Bdata;
static int32    fd1, fd2;	/* file descriptor for input file */
static int32    Ucontrol;	/* UART status register */
static unsigned char aq[UARTBUF], bq[UARTBUF];
static int32    res;
static int32    anum, aind = 0;
static int32    bnum, bind = 0;
static char     wbufa[UARTBUF], wbufb[UARTBUF];
static unsigned wnuma;
static unsigned wnumb;
static FILE    *f1 = NULL, *f2 = NULL;

static char     uarta_sreg, uarta_hreg, uartb_sreg, uartb_hreg;
static uint32   uart_stat_reg;
static uint32   uarta_data, uartb_data;

void            uarta_tx();
void            uartb_tx();
uint32          read_uart();
void            write_uart();
uint32          rtc_counter_read();
void            rtc_scaler_set();
void            rtc_reload_set();
uint32          gpt_counter_read();
void            gpt_scaler_set();
void            gpt_reload_set();
void            timer_ctrl();
void            port_init();
void            uart_irq_start();
void            mec_reset();
void            wdog_start();


/* One-time init */

void
init_sim()
{
    port_init();
}

/* Power-on reset init */

void
reset()
{
    mec_reset();
    uart_irq_start();
    wdog_start();
}

/* IU error mode manager */

int
error_mode(pc)
    uint32          pc;
{

    if ((mec_emr & 0x1) == 0) {
	if (mec_mcr & 0x20) {
	    sys_reset();
	    mec_ersr = 0x8000;
	    printf("Error manager reset - IU in error mode at 0x%08x\n", pc);
	}
    }
}

/* Check memory settings */

void
decode_memcfg()
{
    mem_ramsz = (256 * 1024) << ((mec_memcfg >> 10) & 7);
    mem_banksz = ((mec_memcfg >> 10) & 7) + 18 - 6;
    mem_romsz = (4 * 1024) << ((mec_memcfg >> 18) & 7);
    if (sis_verbose)
	printf("RAM size: %d K, ROM size: %d K, protection bank size: %d K\n",
	       mem_ramsz >> 10, mem_romsz >> 10, 1 << mem_banksz);
}

void
decode_wcr()
{
    mem_ramr_ws = mec_wcr & 3;
    mem_ramw_ws = (mec_wcr >> 2) & 3;
    mem_romr_ws = (mec_wcr >> 4) & 0x0f;
    mem_romw_ws = (mec_wcr >> 8) & 0x0f;
    if (sis_verbose)
	printf("Waitstates = RAM read: %d, RAM write: %d, ROM read: %d, ROM write: %d\n",
	       mem_ramr_ws, mem_ramw_ws, mem_romr_ws, mem_romw_ws);
}

void
decode_mcr()
{
    mem_accprot = (mec_mcr >> 3) & 1;
    if (sis_verbose && mem_accprot)
	printf("Memory access protection enabled\n");
    if (sis_verbose && (mec_mcr & 2))
	printf("Software reset enabled\n");
    if (sis_verbose && (mec_mcr & 1))
	printf("Power-down mode enabled\n");
}

/* Flush ports when simulator stops */

void
sim_stop()
{
#ifdef FAST_UART
    flush_uart();
#endif
}

void
close_port()
{
  if (f1)
    fclose(f1);
  if (f2)
    fclose(f2);
}

void
exit_sim()
{
    close_port();
}

void
mec_reset()
{

    find = 0;
    brk_point = 0;
    watch_point = 0;
    mec_dbg = 0;
    mec_sfsr = 0x078;
    mec_ffar = 0;
    mec_ipr = 0;
    mec_imr = 0x3fff;
    mec_icr = 0;
    mec_ifr = 0;
    mec_memcfg = 0x10000;
    mec_mcr = 0x01b50014;
    mec_wcr = -1;
    mec_mar0 = -1;
    mec_mar1 = -1;
    mec_ersr = 0;		/* MEC error and status register */
    mec_emr = 0x60;		/* MEC error mask register */
    mec_tcr = 0;		/* MEC test comtrol register */

    decode_memcfg();
    decode_wcr();
    decode_mcr();

    posted_irq = 0;
    wnuma = wnumb = 0;
    anum = aind = bnum = bind = 0;

    uart_stat_reg = UARTA_SRE | UARTA_HRE | UARTB_SRE | UARTB_HRE;
    uarta_data = uartb_data = UART_THE | UART_TSE;

    rtc_counter = 0xffffffff;
    rtc_reload = 0xffffffff;
    rtc_scaler = 0xff;
    rtc_enabled = 0;
    rtc_cr = 0;
    rtc_se = 0;
    rtc_cont = 0;

    gpt_counter = 0xffffffff;
    gpt_reload = 0xffffffff;
    gpt_scaler = 0xffff;
    gpt_enabled = 0;
    gpt_cr = 0;
    gpt_se = 0;
    gpt_cont = 0;

    wdog_scaler = 255;
    wdog_rst_delay = 255;
    wdog_counter = 0xffff;
    wdog_rston = 0;
    wdog_status = init;

#ifdef MECREV0
    gpt_irqon = 1;
    rtc_irqon = 1;
#endif

}



int32
mec_intack(level)
    int32           level;
{
    int             irq_test;

    if (sis_verbose) 
	printf("interrupt %d acknowledged\n",level);
    irq_test = mec_tcr & 0x80000;
    if ((irq_test) && (mec_ifr & (1 << level)))
	mec_ifr &= ~(1 << level);
    else
	mec_ipr &= ~(1 << level);
    posted_irq &= ~(1 << level);
#ifdef MECREV0
    if (mecrev0) {
	if (uart_stat_reg & 1)
	    mec_ipr |= (1 << 4);
	if (uart_stat_reg & 0x100)
	    mec_ipr |= (1 << 5);
    }
#endif
}

int32
chk_irq()
{
    int32           i;
    uint32          itmp;

    itmp = ((mec_ipr | mec_ifr) & ~mec_imr) & 0x0fffe;
    if (itmp != 0) {
	for (i = 15; i > 0; i--) {
	    if (((itmp >> i) & 1) != 0) {
		if ((posted_irq & (1 << i)) == 0) {
		    if (sis_verbose) 
			printf("interrupt %d generated\n",i);
		    set_int(i, mec_intack, i);
		    posted_irq |= (1 << i);
		}
	    }
	}
    }
}

void
mec_irq(level)
    int32           level;
{
    mec_ipr |= (1 << level);
    chk_irq();
}

void
set_sfsr(fault, addr, asi, read)
    uint32          fault;
    uint32          addr;
    uint32          asi;
    uint32          read;
{
    mec_ffar = addr;
    mec_sfsr = (fault << 3) | (!read << 15);
    switch (asi) {
    case 8:
	mec_sfsr |= 0x2002;
	break;
    case 9:
	mec_sfsr |= 0x3002;
	break;
    case 0xa:
	mec_sfsr |= 0x0004;
	break;
    case 0xb:
	mec_sfsr |= 0x1004;
	break;
    }
}

int32
chk_brk(addr, asi)
    uint32          addr;
{
    if ((mec_dbg & 0x80000) && (addr == brk_point) &&
	((asi == 9) || (asi == 8))) {
	mec_dbg |= 0x00800000;
	if (mec_dbg & 0x00200000) {
	    set_sfsr(BREAK_EXC, addr, asi, 1);
	    return (1);
	}
    }
    return (0);
}

int32
chk_watch(addr, read, asi)
    uint32          addr;
    uint32          read;
{
    uint32          hit;

    if ((mec_dbg & 0x40000) && (asi != 9) && (asi != 8) &&
	(((mec_dbg & 0x10000) && (read == 0)) || ((mec_dbg & 0x20000) && read))) {
	if (((addr ^ watch_point) &
	     (0xffff0000 | (mec_dbg & 0x0ffff))) == 0) {
	    mec_dbg |= 0x00400000;
	    if (mec_dbg & 0x100000) {
		set_sfsr(WATCH_EXC, addr, asi, read);
		return (1);
	    }
	}
    }
    return (0);
}

int32
mec_read(addr, asi, data)
    uint32          addr;
    uint32          asi;
    uint32         *data;
{

    switch (addr & 0x0ff) {

    case MEC_SFR:
    case MEC_WDOG:
	return (1);
	break;
    case MEC_DBG:
	*data = mec_dbg;
	break;
    case MEC_UARTA:
    case MEC_UARTB:
	if (asi != 0xb)
	    return (1);
	*data = read_uart(addr);
	break;

    case MEC_UART_CTRL:

	*data = read_uart(addr);
	break;

    case MEC_RTC_COUNTER:
	*data = rtc_counter_read();
	break;

    case MEC_GPT_COUNTER:
	*data = gpt_counter_read();
	break;

    case MEC_SFSR:
	*data = mec_sfsr;
	break;

    case MEC_FFAR:
	*data = mec_ffar;
	break;

    case MEC_IPR:
	*data = mec_ipr;
	break;

    case MEC_IMR:
	*data = mec_imr;
	break;

    case MEC_IFR:
	*data = mec_ifr;
	break;

    case SIM_LOAD:
	fname[find] = 0;
	if (find == 0)
	    strcpy(fname, "simload");
	*data = bfd_load(fname);
	find = 0;
	break;

    case MEC_MCR:
	*data = mec_mcr;
	break;

    case MEC_MEMCFG:
	*data = mec_memcfg;
	break;

    case MEC_WCR:
	*data = mec_wcr;
	break;

    case MEC_MAR0:
	*data = mec_mar0;
	break;

    case MEC_MAR1:
	*data = mec_mar1;
	break;

    case MEC_PWDR:
	return (1);
	break;

    default:
	if (sis_verbose)
	    printf("Warning, read from unimplemented MEC register %x\n\r", addr);
	*data = mec_regs[((addr & 0x0ff) >> 2)];
	break;
    }
    return (MOK);
}

int
mec_write(addr, data)
    uint32          addr;
    uint32          data;
{

    switch (addr & 0x0ff) {

    case MEC_SFR:
	if (mec_mcr & 0x2) {
	    sys_reset();
	    mec_ersr = 0x4000;
	    printf(" Software reset issued\n");
	}
	break;

    case MEC_BRK:
	brk_point = data;
	break;

    case MEC_DBG:
	mec_dbg = data;
	break;

    case MEC_WPR:
	watch_point = data;
	break;

    case MEC_UARTA:
    case MEC_UARTB:
    case MEC_UART_CTRL:
	write_uart(addr, data);
	break;

    case MEC_GPT_RELOAD:
	gpt_reload_set(data);
	break;

    case MEC_GPT_SCALER:
	gpt_scaler_set(data);
	break;

    case MEC_TIMER_CTRL:
	timer_ctrl(data);
	break;

    case MEC_RTC_RELOAD:
	rtc_reload_set(data);
	break;

    case MEC_RTC_SCALER:
	rtc_scaler_set(data);
	break;

    case MEC_SFSR:
	mec_sfsr = 0;
	break;

    case MEC_IMR:
	mec_imr = data & 0x7ffe;
	chk_irq();
	break;

    case MEC_ICR:
	mec_icr &= ~data & 0x0fffe;
	break;

    case MEC_IFR:
	mec_ifr = data & 0xfffe;
	chk_irq();
	break;
    case SIM_LOAD:
	fname[find++] = (char) data;
	break;

    case MEC_MCR:
	mec_mcr = data;
	decode_mcr();
	break;

    case MEC_MEMCFG:
	mec_memcfg = data & ~0xC0e08000;
	decode_memcfg();
	break;

    case MEC_WCR:
	mec_wcr = data;
	decode_wcr();
	break;

    case MEC_MAR0:
	mec_mar0 = data;
	break;

    case MEC_MAR1:
	mec_mar1 = data;
	break;

    case MEC_WDOG:
	wdog_scaler = (data >> 16) & 0x0ff;
	wdog_counter = data & 0x0ffff;
	wdog_rst_delay = data >> 24;
	wdog_rston = 0;
	if (wdog_status == stopped)
	    wdog_start();
	wdog_status = enabled;
	break;

    case MEC_TRAPD:
	if (wdog_status == init) {
	    wdog_status = disabled;
	    if (sis_verbose)
		printf("Watchdog disabled\n");
	}
	break;

    case MEC_PWDR:
	if (mec_mcr & 1)
	    wait_for_irq();
	break;

    default:
	if (sis_verbose)
	    printf("Warning, write to unimplemented MEC register %x\n\r",
		   addr);
	mec_regs[((addr & 0x0ffc) >> 2)] = data;
	break;
    }
    return (MOK);
}


/* MEC UARTS */


void
port_init()
{
    int32           pty_remote = 1;

#if !defined _WIN32 && !defined __GO32__
    if ((fd1 = open(uart_dev1, O_RDWR | O_NDELAY | O_NONBLOCK)) < 0) {
	printf("Warning, couldn't open output device %s\n", uart_dev1);
    } else {
	printf("serial port A on %s\n", uart_dev1);
	f1 = fdopen(fd1, "r+");
	setbuf(f1, NULL);
    }
    if ((fd2 = open(uart_dev2, O_RDWR | O_NDELAY | O_NONBLOCK)) < 0) {
	printf("Warning, couldn't open output device %s\n", uart_dev2);
    } else {
	printf("serial port B on %s\n", uart_dev2);
	f2 = fdopen(fd2, "r+");
	setbuf(f2, NULL);
    }
#else
    fd1 = 0;
    fd2 = 0;
    f1 = NULL;
    f2 = NULL;
#endif

    wnuma = wnumb = 0;
}

uint32
read_uart(addr)
    uint32          addr;
{

    unsigned        tmp;

    switch (addr & 0xff) {

    case 0xE0:			/* UART 1 */
#ifdef FAST_UART
	if (aind < anum) {
	    if ((aind + 1) < anum)
		mec_irq(4);
	    return (0x700 | (uint32) aq[aind++]);
	} else {
	  if (f1)
	    anum = fread(aq, 1, UARTBUF, f1);
	  else
	    anum = 0;
	    if (anum > 0) {
		aind = 0;
		if ((aind + 1) < anum)
		    mec_irq(4);
		return (0x700 | (uint32) aq[aind++]);
	    } else {
		return (0x600 | (uint32) aq[aind]);
	    }

	}
#else
	tmp = uarta_data;
	uarta_data &= ~UART_DR;
	uart_stat_reg &= ~UARTA_DR;
	return tmp;
#endif
	break;

    case 0xE4:			/* UART 2 */
#ifdef FAST_UART
	if (bind < bnum) {
	    if ((bind + 1) < bnum)
		mec_irq(5);
	    return (0x700 | (uint32) bq[bind++]);
	} else {
	  if (f2)
	    bnum = fread(bq, 1, UARTBUF, f2);
	  else
	    bnum = 0;
	    if (bnum > 0) {
		bind = 0;
		if ((bind + 1) < bnum)
		    mec_irq(5);
		return (0x700 | (uint32) bq[bind++]);
	    } else {
		return (0x600 | (uint32) bq[bind]);
	    }

	}
#else
	tmp = uartb_data;
	uartb_data &= ~UART_DR;
	uart_stat_reg &= ~UARTB_DR;
	return tmp;
#endif
	break;

    case 0xE8:			/* UART status register	 */
#ifdef FAST_UART
	Ucontrol = 0;
	if (aind < anum) {
	    Ucontrol |= 0x00000001;
	} else {
	  if (f1)
	    anum = fread(aq, 1, UARTBUF, f1);
	  else
	    anum = 0;
	    if (anum > 0) {
		Ucontrol |= 0x00000001;
		aind = 0;
		mec_irq(4);
	    }
	}
	if (bind < bnum) {
	    Ucontrol |= 0x00010000;
	} else {
	  if (f2)
	    bnum = fread(bq, 1, UARTBUF, f2);
	  else
	    bnum = 0;
	    if (bnum > 0) {
		Ucontrol |= 0x00010000;
		bind = 0;
		mec_irq(5);
	    }
	}

	Ucontrol |= 0x00060006;
	return (Ucontrol);
#else
	return (uart_stat_reg);
#endif
	break;
    default:
	if (sis_verbose)
	    printf("Read from unimplemented MEC register (%x)\n", addr);

    }
    return (0);
}

void
write_uart(addr, data)
    uint32          addr;
    uint32          data;
{

    int32           wnum = 0;
    unsigned char   c;

    c = (unsigned char) data;
    switch (addr & 0xff) {

    case 0xE0:			/* UART A */
#ifdef FAST_UART
	if (wnuma < UARTBUF)
	    wbufa[wnuma++] = c;
	else {
	    while (wnuma)
	      if (f1)
		wnuma -= fwrite(wbufa, 1, wnuma, f1);
	      else
		wnuma--;
	    wbufa[wnuma++] = c;
	}
	mec_irq(4);
#else
	if (uart_stat_reg & UARTA_SRE) {
	    uarta_sreg = c;
	    uart_stat_reg &= ~UARTA_SRE;
	    event(uarta_tx, 0, UART_TX_TIME);
	} else {
	    uarta_hreg = c;
	    uart_stat_reg &= ~UARTA_HRE;
	}
#endif
	break;

    case 0xE4:			/* UART B */
#ifdef FAST_UART
	if (wnumb < UARTBUF)
	    wbufb[wnumb++] = c;
	else {
	    while (wnumb)
	      if (f2)
		wnumb -= fwrite(wbufb, 1, wnumb, f2);
	      else
		wnumb--;
	    wbufb[wnumb++] = c;
	}
	mec_irq(5);
#else
	if (uart_stat_reg & UARTB_SRE) {
	    uartb_sreg = c;
	    uart_stat_reg &= ~UARTB_SRE;
	    event(uartb_tx, 0, UART_TX_TIME);
	} else {
	    uartb_hreg = c;
	    uart_stat_reg &= ~UARTB_HRE;
	}
#endif
	break;
    case 0xE8:			/* UART status register */
#ifndef FAST_UART
	if (data & UARTA_CLR) {
	    uart_stat_reg &= 0xFFFF0000;
	    uart_stat_reg |= UARTA_SRE | UARTA_HRE;
	}
	if (data & UARTB_CLR) {
	    uart_stat_reg &= 0x0000FFFF;
	    uart_stat_reg |= UARTB_SRE | UARTB_HRE;
	}
#endif
	break;
    default:
	if (sis_verbose)
	    printf("Write to unimplemented MEC register (%x)\n", addr);

    }
}

flush_uart()
{
    while (wnuma)
      if (f1)
	wnuma -= fwrite(wbufa, 1, wnuma, f1);
      else
	wnuma = 0;
    while (wnumb)
      if (f2)
	wnumb -= fwrite(wbufb, 1, wnumb, f2);
      else
	wnumb = 0;
}



void 
uarta_tx()
{

    while ((f1 ? fwrite(&uarta_sreg, 1, 1, f1) : 1) != 1);
    if (uart_stat_reg & UARTA_HRE) {
	uart_stat_reg |= UARTA_SRE;
    } else {
	uarta_sreg = uarta_hreg;
	uart_stat_reg |= UARTA_HRE;
	event(uarta_tx, 0, UART_TX_TIME);
    }
    mec_irq(4);
}

void 
uartb_tx()
{
    while (fwrite(&uartb_sreg, 1, 1, f2) != 1);
    if (uart_stat_reg & UARTB_HRE) {
	uart_stat_reg |= UARTB_SRE;
    } else {
	uartb_sreg = uartb_hreg;
	uart_stat_reg |= UARTB_HRE;
	event(uartb_tx, 0, UART_TX_TIME);
    }
    mec_irq(5);
}

void
uart_rx(arg)
    SIM_ADDR         arg;
{
    int32           rsize;
    char            rxd;

    rsize = fread(&rxd, 1, 1, f1);
    if (rsize) {
	uarta_data = UART_DR | rxd;
	if (uart_stat_reg & UARTA_HRE)
	    uarta_data |= UART_THE;
	if (uart_stat_reg & UARTA_SRE)
	    uarta_data |= UART_TSE;
	if (uart_stat_reg & UARTA_DR) {
	    uart_stat_reg |= UARTA_OR;
	    mec_irq(7);		/* UART error interrupt */
	}
	uart_stat_reg |= UARTA_DR;
	mec_irq(4);
    }
    rsize = fread(&rxd, 1, 1, f2);
    if (rsize) {
	uartb_data = UART_DR | rxd;
	if (uart_stat_reg & UARTB_HRE)
	    uartb_data |= UART_THE;
	if (uart_stat_reg & UARTB_SRE)
	    uartb_data |= UART_TSE;
	if (uart_stat_reg & UARTB_DR) {
	    uart_stat_reg |= UARTB_OR;
	    mec_irq(7);		/* UART error interrupt */
	}
	uart_stat_reg |= UARTB_DR;
	mec_irq(5);
    }
    event(uart_rx, 0, UART_RX_TIME);
}

void
uart_intr(arg)
    SIM_ADDR         arg;
{
    read_uart(0xE8);		/* Check for UART interrupts every 1000 clk */
    flush_uart();		/* Flush UART ports      */
    event(uart_intr, 0, UART_FLUSH_TIME);
}


void
uart_irq_start()
{
#ifdef FAST_UART
    event(uart_intr, 0, UART_FLUSH_TIME);
#else
    event(uart_rx, 0, UART_RX_TIME);
#endif
}

/* Watch-dog */

void
wdog_intr(arg)
    SIM_ADDR         arg;
{
    if (wdog_status == disabled) {
	wdog_status = stopped;
    } else {

	if (wdog_counter) {
	    wdog_counter--;
	    event(wdog_intr, 0, wdog_scaler + 1);
	} else {
	    if (wdog_rston) {
		printf("Watchdog reset!\n");
		sys_reset();
		mec_ersr = 0xC000;
	    } else {
		mec_irq(15);
		wdog_rston = 1;
		wdog_counter = wdog_rst_delay;
		event(wdog_intr, 0, wdog_scaler + 1);
	    }
	}
    }
}

void
wdog_start()
{
    event(wdog_intr, 0, wdog_scaler + 1);
    if (sis_verbose)
	printf("Watchdog started, scaler = %d, counter = %d\n",
	       wdog_scaler, wdog_counter);
}


/* MEC timers */


void
rtc_intr(arg)
    SIM_ADDR         arg;
{
    if (rtc_counter == 0) {
#ifdef MECREV0
	if (mecrev0) {
	    if (rtc_cr) {
		rtc_counter = rtc_reload;
		mec_irq(13);
	    } else {
		rtc_cont = 0;
		if (rtc_irqon) {
		    mec_irq(13);
		    rtc_irqon = 0;
		} else {
		    if (sis_verbose)
			printf("RTC interrupt lost (MEC rev.0)\n");
		}
	    }
	} else {
	    mec_irq(13);
	    if (rtc_cr)
		rtc_counter = rtc_reload;
	    else
		rtc_cont = 0;
	}

#else

	mec_irq(13);
	if (rtc_cr)
	    rtc_counter = rtc_reload;
	else
	    rtc_cont = 0;
#endif

    } else
	rtc_counter -= 1;
    if (rtc_se && rtc_cont) {
	event(rtc_intr, 0, rtc_scaler + 1);
	rtc_enabled = 1;
    } else {
	if (sis_verbose)
	    printf("RTC stopped\n\r");
	rtc_enabled = 0;
    }
}

void
rtc_start()
{
    if (sis_verbose)
	printf("RTC started (period %d)\n\r", rtc_scaler + 1);
    event(rtc_intr, 0, rtc_scaler + 1);
    rtc_enabled = 1;
}

uint32
rtc_counter_read()
{
    return (rtc_counter);
}

void
rtc_scaler_set(val)
    uint32          val;
{
    rtc_scaler = val & 0x0ff;	/* eight-bit scaler only */
}

void
rtc_reload_set(val)
    uint32          val;
{
    rtc_reload = val;
}

void
gpt_intr(arg)
    SIM_ADDR         arg;
{
    if (gpt_counter == 0) {
#ifdef MECREV0
	if (mecrev0) {
	    if (gpt_cr) {
		gpt_counter = gpt_reload;
		mec_irq(12);
	    } else {
		gpt_cont = 0;
		if (gpt_irqon) {
		    mec_irq(12);
		    gpt_irqon = 0;
		} else {
		    if (sis_verbose)
			printf("GPT interrupt lost (MEC rev.0)\n");
		}
	    }
	} else {
	    mec_irq(12);
	    if (gpt_cr)
		gpt_counter = gpt_reload;
	    else
		gpt_cont = 0;
	}

#else
	mec_irq(12);
	if (gpt_cr)
	    gpt_counter = gpt_reload;
	else
	    gpt_cont = 0;
#endif
    } else
	gpt_counter -= 1;
    if (gpt_se && gpt_cont) {
	event(gpt_intr, 0, gpt_scaler + 1);
	gpt_enabled = 1;
    } else {
	if (sis_verbose)
	    printf("GPT stopped\n\r");
	gpt_enabled = 0;
    }
}

void
gpt_start()
{
    if (sis_verbose)
	printf("GPT started (period %d)\n\r", gpt_scaler + 1);
    event(gpt_intr, 0, gpt_scaler + 1);
    gpt_enabled = 1;
}

uint32
gpt_counter_read()
{
    return (gpt_counter);
}

void
gpt_scaler_set(val)
    uint32          val;
{
    gpt_scaler = val & 0x0ffff;	/* 16-bit scaler */
}

void
gpt_reload_set(val)
    uint32          val;
{
    gpt_reload = val;
}

void
timer_ctrl(val)
    uint32          val;
{

#ifdef MECREV0
    if ((mecrev0) && (val & 0x500))
	rtc_irqon = 1;
#endif

    rtc_cr = ((val & TCR_TCRCR) != 0);
    if (val & TCR_TCRCL) {
	rtc_counter = rtc_reload;
	rtc_cont = 1;
    }
    if (val & TCR_TCRSL) {
	rtc_cont = 1;
    }
    rtc_se = ((val & TCR_TCRSE) != 0);
    if (rtc_cont && rtc_se && (rtc_enabled == 0))
	rtc_start();

#ifdef MECREV0
    if ((mecrev0) && (val & 0x5))
	gpt_irqon = 1;
#endif

    gpt_cr = (val & TCR_GACR);
    if (val & TCR_GACL) {
	gpt_counter = gpt_reload;
	gpt_cont = 1;
    }
    if (val & TCR_GACL) {
	gpt_cont = 1;
    }
    gpt_se = (val & TCR_GASE) >> 2;
    if (gpt_cont && gpt_se && (gpt_enabled == 0))
	gpt_start();
}


/* Memory emulation */

/* ROM size 512 Kbyte */
#define ROM_SZ	 	0x080000

/* RAM size 4 Mbyte */
#define RAM_START 	0x02000000
#define RAM_END 	0x02400000
#define RAM_MASK 	0x003fffff

/* MEC registers */
#define MEC_START 	0x01f80000
#define MEC_END 	0x01f80100

/* Memory exception waitstates */
#define MEM_EX_WS 	1

/* ERC32 always adds one waitstate during ldd/std */
#define LDD_WS 1
#define STD_WS 1

extern int32    sis_verbose;

static uint32   romb[ROM_SZ / 4];
static uint32   ramb[(RAM_END - RAM_START) / 4];

int
memory_read(asi, addr, data, ws)
    int32           asi;
    uint32          addr;
    uint32         *data;
    int32          *ws;
{
    int32           mexc;
    uint32         *mem;

#ifdef MECBRK

    if (mec_dbg & 0x80000) {
	if (chk_brk(addr, asi)) {
	    *ws = MEM_EX_WS;
	    return (1);
	}
    }
    if (mec_dbg & 0x40000) {
	if (chk_watch(addr, 1, asi)) {
	    *ws = MEM_EX_WS;
	    return (1);
	}
    }
#endif

    if (addr < mem_romsz) {
	*data = romb[addr >> 2];
	*ws = mem_romr_ws;
	return (0);
    } else if ((addr >= RAM_START) && (addr < (RAM_START + mem_ramsz))) {
	*data = ramb[(addr & RAM_MASK) >> 2];
	*ws = mem_ramr_ws;
	return (0);
    } else if ((addr >= MEC_START) && (addr < MEC_END)) {
	mexc = mec_read(addr, asi, data);
	if (mexc) {
	    set_sfsr(MEC_ACC, addr, asi, 1);
	    *ws = MEM_EX_WS;
	} else {
	    *ws = 0;
	}
	return (mexc);
    }
    printf("Memory exception at %x (illegal address)\n", addr);
    set_sfsr(UIMP_ACC, addr, asi, 1);
    *ws = MEM_EX_WS;
    return (1);
}

int
memory_write(asi, addr, data, sz, ws)
    int32           asi;
    uint32          addr;
    uint32         *data;
    int32           sz;
    int32          *ws;
{
    uint32          byte_addr;
    uint32          byte_mask;
    uint32          waddr;
    uint32          bank;
    int32           mexc;

#ifdef MECBRK
    if (mec_dbg & 0x40000) {
	if (chk_watch(addr, 0, asi)) {
	    *ws = MEM_EX_WS;
	    return (1);
	}
    }
#endif

    if ((addr >= RAM_START) && (addr < (RAM_START + mem_ramsz))) {
	if (mem_accprot) {
	    bank = (addr & RAM_MASK) >> mem_banksz;
	    if (bank < 32
		? !((1 << bank) & mec_mar0)
		: !((1 << (bank - 32) & mec_mar1))) {
		printf("Memory access protection error at %x\n", addr);
		set_sfsr(PROT_EXC, addr, asi, 0);
		*ws = MEM_EX_WS;
		return (1);
	    }
	}
	*ws = mem_ramw_ws;
	waddr = (addr & RAM_MASK) >> 2;
	switch (sz) {
	case 0:
	    byte_addr = addr & 3;
	    byte_mask = 0x0ff << (24 - (8 * byte_addr));
	    ramb[waddr] = (ramb[waddr] & ~byte_mask)
		| ((*data & 0x0ff) << (24 - (8 * byte_addr)));
	    break;
	case 1:
	    byte_addr = (addr & 2) >> 1;
	    byte_mask = 0x0ffff << (16 - (16 * byte_addr));
	    ramb[waddr] = (ramb[waddr] & ~byte_mask)
		| ((*data & 0x0ffff) << (16 - (16 * byte_addr)));
	    break;
	case 2:
	    ramb[waddr] = *data;
	    break;
	case 3:
	    ramb[waddr] = data[0];
	    ramb[waddr + 1] = data[1];
	    *ws += mem_ramw_ws + STD_WS;
	    break;
	}
	return (0);
    } else if ((addr >= MEC_START) && (addr < MEC_END)) {
	if ((sz != 2) || (asi != 0xb)) {
	    set_sfsr(MEC_ACC, addr, asi, 0);
	    *ws = MEM_EX_WS;
	    return (1);
	}
	mexc = mec_write(addr, *data);
	if (mexc) {
	    set_sfsr(MEC_ACC, addr, asi, 0);
	    *ws = MEM_EX_WS;
	} else {
	    *ws = 0;
	}
	return (mexc);

    }
    *ws = MEM_EX_WS;
    set_sfsr(UIMP_ACC, addr, asi, 0);
    return (1);
}

unsigned char  *
get_mem_ptr(addr, size)
    uint32          addr;
    uint32          size;
{
    char           *bram, *brom;

    brom = (char *) romb;
    bram = (char *) ramb;
    if ((addr + size) < ROM_SZ) {
	return (&brom[addr]);
    } else if ((addr >= RAM_START) && ((addr + size) < RAM_END)) {
	return (&bram[(addr & RAM_MASK)]);
    }
    return ((char *) -1);
}

int
sis_memory_write(addr, data, length)
    uint32          addr;
    char           *data;
    uint32          length;
{
    char           *mem;
    uint32          i;

    if ((mem = get_mem_ptr(addr, length)) == ((char *) -1))
	return (0);
#ifdef HOST_LITTLE_ENDIAN
    for (i = 0; i < length; i++) {
	mem[i ^ 0x3] = data[i];
    }
#else
    memcpy(mem, data, length);
#endif
    return (length);
}

int
sis_memory_read(addr, data, length)
    uint32          addr;
    char           *data;
    uint32          length;
{
    char           *mem;
    int		i;

    if ((mem = get_mem_ptr(addr, length)) == ((char *) -1))
	return (0);

#ifdef HOST_LITTLE_ENDIAN
    for (i = 0; i < length; i++) {
	data[i] = mem[i ^ 0x3];
    }
#else
    memcpy(data, mem, length);
#endif
    return (length);
}
