/*	$OpenBSD: sysconreg.h,v 1.1 1999/09/27 18:43:25 smurph Exp $ */

/*
 * Memory map for SYSCON found in mvme188 board set.
 * No specific chips are found here like the PCCTWO 
 * on MVME1x7. All chips are included in this one
 * map/device so that devices don't run rampant in 
 * the config files.  I may change this later XXX smurph.
 */

#include <machine/board.h>

struct sysconreg {
   volatile unsigned int *ien0;
   volatile unsigned int *ien1;
   volatile unsigned int *ien2;
   volatile unsigned int *ien3;
   volatile unsigned int *ienall;
   volatile unsigned int *ist;
   volatile unsigned int *setswi; 
   volatile unsigned int *clrswi; 
   volatile unsigned int *istate;
   volatile unsigned int *clrint;
   volatile unsigned char *global0;
   volatile unsigned char *global1;
   volatile unsigned char *global2;
   volatile unsigned char *global3;
   volatile unsigned int *ucsr;
   volatile unsigned int *glbres;
   volatile unsigned int *ccsr;
   volatile unsigned int *error;
   volatile unsigned int *pcnfa;
   volatile unsigned int *pcnfb;
   volatile unsigned int *extad;
   volatile unsigned int *extam;
   volatile unsigned int *whoami;
   volatile unsigned int *wmad;
   volatile unsigned int *rmad;
   volatile unsigned int *wvad;
   volatile unsigned int *rvad;
   volatile unsigned int *cio_portc;
   volatile unsigned int *cio_portb;
   volatile unsigned int *cio_porta;
   volatile unsigned int *cio_ctrl;
}; 

extern struct sysconreg *sys_syscon;

/*
 * Vectors we use
 */
#define SYSCV_ABRT 		0x110
#define SYSCV_SYSF 		0x111
#define SYSCV_ACF 		0x112
#define SYSCV_SCC 		0x55
#define SYSCV_TIMER4    0x56
#define SYSCV_TIMER3    0x57
#define SYSCV_TIMER2    0x58
#define SYSCV_TIMER1    0x59


