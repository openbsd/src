/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include "bug.h"

#define _INCHR		"00"
#define _INSTAT		"01"
#define _INLN		"02"
#define _READSTR	"03"
#define _READLN		"04"
#define _DSKRD		"16"
#define _DSKWR		"17"
#define _DSKCFIG	"18"
#define _DSKFMT		"20"
#define _DSKCTRL	"12"
#define _OUTCHR		"32"
#define _OUTSTR		"33"
#define _OUTLN		"34"
#define _WRITE		"35"
#define _WRITELN	"36"
#define _DELAY		"67"
#define _RTC_RD		"83"
#define _RETURN		"99"
#define _BRD_ID		"112"

/* BUG  -  tty routines */

#define BUG_CALL(x)	\
	asm volatile ("or r9,r0," x); \
	asm volatile ("tb0 0,r0,496");

char bug_inchr()
{
	register char a;
	asm volatile ("sub r31,r31,4");
	BUG_CALL(_INCHR);
	asm volatile ("or %0,r0,r2" :  "=r" (a));
	return a;
}

/* returns 0 if no characters ready to read */
int bug_instat()
{
	short ret;
	BUG_CALL(_INSTAT);
	asm volatile ("or %0,r0,r2" :  "=r" (ret));
	return (!(ret & 0x4));
	
}

void bug_outchr(char a)
{
	asm volatile ("or r2, r0, %0" :  :"r" (a));
	BUG_CALL(_OUTCHR);
	return;
}

void bug_outstr(char *pstrb, char *pstre)
{
	asm volatile ("or r2,r0,%0": : "r" (pstrb) );
	asm volatile ("or r3,r0,%0": : "r" (pstre) );
	BUG_CALL(_OUTSTR);
	return;
}

void bug_outln(char *pstrb, char *pstre)
{
	asm volatile ("or r2,r0,%0": : "r" (pstrb) );
	asm volatile ("or r3,r0,%0": : "r" (pstre) );
	BUG_CALL(_OUTLN);
	return;
}

/* BUG - disk routines */

/* returns 0: success, nonzero: error */
int bug_diskrd(bug_dskio *arg)
{
	int ret;
	asm volatile ("or r2,r0,%0": : "r" (arg) );
	BUG_CALL(_DSKRD);
	return (!(ret & 0x4));
}
/* returns 0: success, nonzero: error */
int bug_diskwr(bug_dskio *arg)
{
	int ret;
	asm volatile ("or r2,r0,%0": : "r" (arg) );
	BUG_CALL(_DSKWR);
	return (!(ret & 0x4));
}
#ifdef NOTYET
bug_diskcfig()
{
	
}
bug_diskfmt(){}
bug_diskctrl(){}
#endif

/* BUG - timing routine */

void bug_delay(int delay_msec)
{
	asm volatile ("or r2,r0,%0": : "r" (delay_msec) );
	BUG_CALL(_DELAY);
	return ;
}

/* BUG - return to bug routine */

void bug_return()
{
	BUG_CALL(_RETURN);
	/*NOTREACHED*/
}

/* BUG - query board routines */

struct bug_brdid *bug_brdid()
{
	struct bug_brdid *pbrd_id;
	BUG_CALL(_BRD_ID);
	asm volatile ("or %0,r0,r2": "=r" (pbrd_id):);
	return pbrd_id;
}
void bug_rtc_rd(struct bug_time *ptime)
{
	asm volatile ("or r2,r0,%0": : "r" (ptime));
	BUG_CALL(_RTC_RD);
	return;
}
