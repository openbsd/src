/*	$OpenBSD: bug.c,v 1.4 1999/09/27 20:30:31 smurph Exp $ */

/*
 * Copyright (c) 1995 Dale Rahn.
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
 *	This product includes software developed by Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/param.h>
#include <machine/prom.h>

/* flag to traphandler to signify prom call. presumes splhigh() */
extern volatile int promcall;

/* tty routines */
char
bug_inchr()
{
	int s = splhigh();
	volatile char a;

	promcall = 1;
	asm volatile ("subql #2,sp");
	MVMEPROM_CALL(MVMEPROM_INCHR);
	asm volatile ("nop");
	asm volatile ("movb sp@+,%0" : "=d" (a));
	promcall = 0;
	splx(s);
	return (a);
}

/* returns 0 if no characters ready to read */
int
bug_instat()
{
	int s = splhigh();
	volatile short ret;

	promcall = 1;
	MVMEPROM_CALL(MVMEPROM_INSTAT);
	asm volatile ("nop");
	asm volatile ("movw ccr,%0" : "=d" (ret));
	promcall = 0;
	splx(s);
	return (!(ret & 0x4));
}

void
bug_outchr(a)
	char a;
{
	int s = splhigh();

	promcall = 1;
	asm volatile ("movb %0,sp@-" :: "d" (a));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
	asm volatile ("nop");
	promcall = 0;
	splx(s);
}

void
bug_outstr(pstrb, pstre)
	char *pstrb;
	char *pstre;
{
	int s = splhigh();

	promcall = 1;
	asm volatile ("movl %0,sp@-" :: "d" (pstre));
	asm volatile ("movl %0,sp@-" :: "d" (pstrb));
	MVMEPROM_CALL(MVMEPROM_OUTSTR);
	promcall = 0;
	splx(s);
}

void
bug_outln(pstrb, pstre)
	char *pstrb;
	char *pstre;
{
	int s = splhigh();

	promcall = 1;
	asm volatile ("movl %0,sp@-" :: "d" (pstre));
	asm volatile ("movl %0,sp@-" :: "d" (pstrb));
	MVMEPROM_CALL(MVMEPROM_OUTSTRCRLF);
	promcall = 0;
	splx(s);
}

/* BUG - disk routines */

#if 0
/* returns 0: success, nonzero: error */
u_int bug_drdcnt = 0;
int
bug_diskrd(arg)
	bug_dskio *arg;
{
	volatile int ret;

	promcall = 1;
	bug_drdcnt++;
	asm volatile ("movl %0, sp@-" :: "d" (arg));
	MVMEPROM_CALL(MVMEPROM_DSKRD);
	asm volatile ("nop");
	asm volatile ("movw ccr,%0" : "=d" (ret));
	promcall = 0;
	return (!(ret & 0x4));
}

/* returns 0: success, nonzero: error */
u_int bug_dwrcnt = 0;
int
bug_diskwr(arg)
	bug_dskio *arg;
{
	volatile int ret;

	promcall = 1;
	bug_dwrcnt ++;
	asm volatile ("movl %0, sp@-" :: "d" (arg));
	MVMEPROM_CALL(MVMEPROM_DSKWR);
	asm volatile ("nop");
	asm volatile ("movw ccr,%0" : "=d" (ret));
	promcall = 0;
	return (!(ret & 0x4));
}

bug_diskcfig()
{
}

bug_diskfmt()
{
}

bug_diskctrl()
{
}
#endif

/* BUG - timing routine */
void
bug_delay(delay_msec)
	int delay_msec;
{
	int s = splhigh();

	promcall = 1;
	asm volatile ("movl %0,sp@-" :: "d" (delay_msec));
	MVMEPROM_CALL(MVMEPROM_DELAY);
	asm volatile ("nop");
	promcall = 0;
	splx(s);
}

/* BUG - return to bug routine */
void
bug_return()
{
	promcall = 1;
	MVMEPROM_CALL(MVMEPROM_EXIT);
	promcall = 0;
	/*NOTREACHED*/
}

/* BUG - query board routines */
struct bug_brdid *
bug_brdid()
{
	struct bug_brdid *pbrd_id;

	promcall = 1;
	asm volatile ("clrl sp@-");
	MVMEPROM_CALL(MVMEPROM_GETBRDID);
	asm volatile ("movl sp@+,%0" : "=d" (pbrd_id):);
	promcall = 0;
	return (pbrd_id);
}

void
bug_rtc_rd(ptime)
	struct bug_time *ptime;
{
	promcall = 1;
	asm volatile ("movl %0,sp@-" :: "a" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
	asm volatile ("nop");
	promcall = 0;
}

int asm_callbuf[4];

void
bug_stat()
{
	char val[] = "|/-\\";
	static int cnt = 0;

	bug_outchr('\b');
	bug_outchr(val[cnt]);
	cnt = (cnt + 1) % (sizeof(val) -1);
}

void
asm_bug_stat()
{
	asm volatile ("movl a0,_asm_callbuf+0");
	asm volatile ("movl a1,_asm_callbuf+4");
	asm volatile ("movl d0,_asm_callbuf+8");
	asm volatile ("movl d1,_asm_callbuf+12");
	
	bug_stat();

	asm volatile ("movl _asm_callbuf+0,a0");
	asm volatile ("movl _asm_callbuf+4,a1");
	asm volatile ("movl _asm_callbuf+8,d0");
	asm volatile ("movl _asm_callbuf+12,d1");
}





