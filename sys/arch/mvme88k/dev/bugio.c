/*	$OpenBSD: bugio.c,v 1.9 2002/03/05 22:11:37 miod Exp $ */
/*  Copyright (c) 1998 Steve Murphree, Jr. */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bugio.h>
#include <machine/prom.h>

register_t ossr0, ossr1, ossr2, ossr3;
register_t bugsr0, bugsr1, bugsr2, bugsr3;

#define	BUGCTXT()							\
{									\
	__asm__ __volatile__ ("ldcr %0, cr17" : "=r" (ossr0));		\
	__asm__ __volatile__ ("ldcr %0, cr18" : "=r" (ossr1));		\
	__asm__ __volatile__ ("ldcr %0, cr19" : "=r" (ossr2));		\
	__asm__ __volatile__ ("ldcr %0, cr20" : "=r" (ossr3));		\
									\
	__asm__ __volatile__ ("stcr %0, cr17" :: "r"(bugsr0));		\
	__asm__ __volatile__ ("stcr %0, cr18" :: "r"(bugsr1));		\
	__asm__ __volatile__ ("stcr %0, cr19" :: "r"(bugsr2));		\
	__asm__ __volatile__ ("stcr %0, cr20" :: "r"(bugsr3));		\
}

#define	OSCTXT()							\
{									\
	__asm__ __volatile__ ("ldcr %0, cr17" : "=r" (bugsr0)::		\
	    "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",		\
	    "r9", "r10", "r11", "r12", "r13");				\
	__asm__ __volatile__ ("ldcr %0, cr18" : "=r" (bugsr1));		\
	__asm__ __volatile__ ("ldcr %0, cr19" : "=r" (bugsr2));		\
	__asm__ __volatile__ ("ldcr %0, cr20" : "=r" (bugsr3));		\
									\
	__asm__ __volatile__ ("stcr %0, cr17" :: "r"(ossr0));		\
	__asm__ __volatile__ ("stcr %0, cr18" :: "r"(ossr1));		\
	__asm__ __volatile__ ("stcr %0, cr19" :: "r"(ossr2));		\
	__asm__ __volatile__ ("stcr %0, cr20" :: "r"(ossr3));		\
}

static void
bugpcrlf(void)
{
	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_OUTCRLF);
	OSCTXT();
}

void
buginit()
{
	__asm__ __volatile__ ("ldcr %0, cr17" : "=r" (bugsr0));
	__asm__ __volatile__ ("ldcr %0, cr18" : "=r" (bugsr1));
	__asm__ __volatile__ ("ldcr %0, cr19" : "=r" (bugsr2));
	__asm__ __volatile__ ("ldcr %0, cr20" : "=r" (bugsr3));
}

char
buginchr(void)
{
	register int cc;
	int ret;
	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_INCHR);
	__asm__ __volatile__ ("or %0,r0,r2" : "=r" (cc) : );
	ret = cc;
	OSCTXT();
	return ((char)ret & 0xFF);
}

void
bugoutchr(unsigned char c)
{
	unsigned char cc;

	if ((cc = c) == '\n') {
		bugpcrlf();
		return;
	}

	BUGCTXT();
	__asm__ __volatile__ ("or r2,r0,%0" : : "r" (cc));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
	OSCTXT();
}

/* return 1 if not empty else 0 */
int
buginstat(void)
{
	register int ret;

	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_INSTAT);
	__asm__ __volatile__  ("or %0,r0,r2" : "=r" (ret) : );
	OSCTXT();
	return (ret & 0x4 ? 0 : 1);
}

void
bugoutstr(char *s, char *se)
{
	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_OUTSTR);
	OSCTXT();
}

void
bugrtcrd(struct mvmeprom_time *rtc)
{
	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
	OSCTXT();
}

void
bugreturn(void)
{
	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_EXIT);
	OSCTXT();
}

void
bugbrdid(struct mvmeprom_brdid *id)
{
	struct mvmeprom_brdid *ptr;

	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_GETBRDID);
	__asm__ __volatile__ ("or %0,r0,r2" : "=r" (ptr) : );
	OSCTXT();

	bcopy(ptr, id, sizeof(struct mvmeprom_brdid));
}
