/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include "bug.h"

#define _INCHR		"0x00"
#define _INSTAT		"0x01"
#define _INLN		"0x02"
#define _READSTR	"0x03"
#define _READLN		"0x04"
#define _OUTCHR		"0x20"
#define _OUTSTR		"0x21"
#define _OUTLN		"0x22"
#define _DSKRD		"0x10"
#define _DSKWR		"0x11"
#define _DSKCFIG	"0x12"
#define _DSKFMT		"0x14"
#define _DSKCTRL	"0x15"
#define _WRITE		"0x23"
#define _WRITELN	"0x24"
#define _DELAY		"0x43"
#define _RTC_RD		"0x53"
#define _RETURN		"0x63"
#define _BRD_ID		"0x70"

/* BUG  -  tty routines */

#define BUG_CALL(x)	\
	asm volatile ("addi r10,r0," x); \
	asm volatile ("sc");

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
