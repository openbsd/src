
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <machine/board.h>
#include <machine/bug.h>
#include <machine/pcctworeg.h>

extern u_int *pcc_io_base;
extern const u_int timer_reload;
void	setstatclockrate (int hzrate)
{
}       

resettodr()
{
}

int
hexdectodec(unsigned char n)
{

	return(((n>>4)&0x0F)*10 + (n&0x0F));
}

#define STARTOFTIME 1970
#define FEBRUARY 2
#define leapyear(year) (((year)%4==0) && ((year)%100) != 0 || ((year)%400) == 0)
#define days_in_year(year) (leapyear((year)) ? 366 : 365)
#define   days_in_month(a)    (month_days[(a) - 1])

static int month_days[12] = {
     31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
}; 


inittodr(time_t base)
{
	struct bugrtc rtc;
	u_long sec, min, hour, day, month, year;
	u_long i, tmp, timebuf;

	/* ignore suggested time, use realtime clock via bug */
	bugrtcrd(&rtc);
	sec	= hexdectodec(rtc.s);
	min	= hexdectodec(rtc.m);
	hour	= hexdectodec(rtc.H);
	day	= hexdectodec(rtc.D);
	month	= hexdectodec(rtc.M);
	year	= hexdectodec(rtc.Y) + 1900;

	tmp = 0;
	for (i = STARTOFTIME; i < year; i++) {
		tmp += days_in_year(i);
	}
	for (i = 1; i < month; i++) {
		tmp += days_in_month(i);
	}
	if (leapyear(year) && month > FEBRUARY) {
		tmp++;
	}
	printf("date yy mm dd hh mm.ss:%02d %02d %02d %02d %02d.%02d:",
		year,month,day,hour,min, sec);
	tmp += (day -1);
	timebuf = (((tmp * 24 + hour) * 60 + min) * 60 + sec);
	printf(" epochsec %d\n",timebuf);
	time.tv_sec = timebuf;
	time.tv_usec = 0;
}

clkread()
{
}

cpu_initclocks()
{
#if 0
	u_int *io_base;
	io_base = 0xfffe1000; /* should really be return of virtmem alloc */
	/*
	io_base = pcc_io_base;
	*/
	/* timer 2 setup */
	PCC_TIMER2_PRE(io_base) = timer_reload;
	PCC_TIMER2_CTR(io_base) = 0x7;
	PCC_TIMER2_ICR(io_base) = 0x8e;
#endif
}

/*
 * Clock interrupts.
 */
int
clockintr(cap)
	void *cap;
{
#if 0
	volatile register unsigned char icr;
	/* clear clock interrupt */
	asm ("ld.b %0,%1" : "=r" (icr) : "" (TIMER2ICR));
	icr |= ICLR;
	asm ("st.b %0,%1" : "=r" (icr) : "" (TIMER2ICR));

	/* read the limit register to clear the interrupt */
#endif /* 0 */
	hardclock((struct clockframe *)cap);

	return (1);
}
