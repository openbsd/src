/*	$OpenBSD: sysconreg.h,v 1.3 2002/01/14 21:34:39 miod Exp $ */

/*
 * Memory map for SYSCON found in mvme188 board set.
 * No specific chips are found here like the PCCTWO 
 * on MVME1x7. All chips are included in this one
 * map/device so that devices don't run rampant in 
 * the config files.  I may change this later XXX smurph.
 */

#include <machine/board.h>

struct sysconreg {
	unsigned int *volatile ien0;
	unsigned int *volatile ien1;
	unsigned int *volatile ien2;
	unsigned int *volatile ien3;
	unsigned int *volatile ienall;
	unsigned int *volatile ist;
	unsigned int *volatile setswi; 
	unsigned int *volatile clrswi; 
	unsigned int *volatile istate;
	unsigned int *volatile clrint;
	unsigned char *volatile global0;
	unsigned char *volatile global1;
	unsigned char *volatile global2;
	unsigned char *volatile global3;
	unsigned int *volatile ucsr;
	unsigned int *volatile glbres;
	unsigned int *volatile ccsr;
	unsigned int *volatile error;
	unsigned int *volatile pcnfa;
	unsigned int *volatile pcnfb;
	unsigned int *volatile extad;
	unsigned int *volatile extam;
	unsigned int *volatile whoami;
	unsigned int *volatile wmad;
	unsigned int *volatile rmad;
	unsigned int *volatile wvad;
	unsigned int *volatile rvad;
	unsigned int *volatile cio_portc;
	unsigned int *volatile cio_portb;
	unsigned int *volatile cio_porta;
	unsigned int *volatile cio_ctrl;
}; 

extern struct sysconreg *sys_syscon;

/*
 * Vectors we use
 */
#define SYSCV_ABRT	0x52
#define SYSCV_SYSF	0x53
#define SYSCV_ACF 	0x54
#define SYSCV_SCC 	0x55
#define SYSCV_TIMER4    0x56
#define SYSCV_TIMER3    0x57
#define SYSCV_TIMER2    0x58
#define SYSCV_TIMER1    0x59


