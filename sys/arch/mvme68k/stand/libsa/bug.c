/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#define MVMEPROM_CALL(x) \
	asm volatile (__CONCAT("trap #15; .short ", __STRING(x)) )

/* returns 0 if no characters ready to read */
int
mvmeprom_instat()
{
	u_short ret;

	MVMEPROM_CALL(MVMEPROM_INSTAT);
	asm volatile ("movew ccr,%0": "=d" (ret));
	return (!(ret & 0x4));
}

void
mvmeprom_outstr(start, end)
	char *start, *end;
{
	asm volatile ("movl %0, sp@-" : "=a" (start));
	asm volatile ("movl %0, sp@-" : "=a" (end));
	MVMEPROM_CALL(MVMEPROM_OUTSTR);
}

void
mvmeprom_outln(start, end)
	char *start, *end;
{
	asm volatile ("movl %0, sp@-" : "=a" (start));
	asm volatile ("movl %0, sp@-" : "=a" (end));
	MVMEPROM_CALL(MVMEPROM_OUTSTRCRLF);
}

/* returns 0: success, nonzero: error */
int
mvmeprom_diskrd(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	asm volatile ("movel %0, sp@-"::"d" (arg));
	MVMEPROM_CALL(MVMEPROM_DSKRD);
	asm volatile ("movew ccr,%0": "=d" (ret));
	return (!(ret & 0x4));
}

/* returns 0: success, nonzero: error */
int
mvmeprom_diskwr(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	asm volatile ("movel %0, sp@-"::"d" (arg));
	MVMEPROM_CALL(MVMEPROM_DSKWR);
	asm volatile ("movew ccr,%0": "=d" (ret));
	return (!(ret & 0x4));
}

#ifdef NOTYET
mvmeprom_diskcfig() {}
mvmeprom_diskfmt(){}
mvmeprom_diskctrl(){}
#endif

/* BUG - timing routine */
void
mvmeprom_delay(msec)
	int msec;
{
	asm volatile ("movel %0,sp@-" :  :"d" (msec));
	MVMEPROM_CALL(MVMEPROM_DELAY);
}

/* BUG - return to bug routine */
void
mvmeprom_return()
{
	MVMEPROM_CALL(MVMEPROM_EXIT);
	/*NOTREACHED*/
}

/* BUG - query board routines */
struct mvmeprom_brdid *
mvmeprom_getbrdid()
{
	struct mvmeprom_brdid *id;

	asm volatile ("clrl sp@-");
	MVMEPROM_CALL(MVMEPROM_GETBRDID);
	asm volatile ("movel sp@+,%0": "=d" (id):);
	return (id);
}

void
mvmeprom_rtc_rd(ptime)
	struct mvmeprom_time *ptime;
{
	asm volatile ("movel %0,sp@-" :  :"a" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
}
