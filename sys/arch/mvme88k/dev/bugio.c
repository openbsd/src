/*	$OpenBSD: bugio.c,v 1.2 1998/12/15 05:52:29 smurph Exp $ */
/*  Copyright (c) 1998 Steve Murphree, Jr. */
#include <machine/bugio.h>

#define INCHR	"0x0000"
#define INSTAT	"0x0001"
#define INLN	"0x0002"
#define READSTR	"0x0003"
#define READLN	"0x0004"
#define	DSKRD	"0x0010"
#define	DSKWR	"0x0011"
#define	DSKCFIG	"0x0012"
#define	NETCTRL	"0x001D"
#define	OUTCHR	"0x0020"
#define	OUTSTR	"0x0021"
#define	PCRLF	"0x0026"
#define	TMDISP	"0x0042"
#define	DELAY	"0x0043"
#define	RTC_DSP	"0x0052"
#define	RTC_RD	"0x0053"
#define	RETURN	"0x0063"
#define	BRD_ID	"0x0070"
#define BUGTRAP	"0x01F0"

int ossr0, ossr1, ossr2, ossr3;
int bugsr0, bugsr1, bugsr2, bugsr3;

#define	BUGCTXT()						\
{								\
	asm volatile ("ldcr %0, cr17" : "=r" (ossr0));		\
	asm volatile ("ldcr %0, cr18" : "=r" (ossr1));		\
	asm volatile ("ldcr %0, cr19" : "=r" (ossr2));		\
	asm volatile ("ldcr %0, cr20" : "=r" (ossr3));		\
								\
	asm volatile ("stcr %0, cr17" :: "r"(bugsr0));		\
	asm volatile ("stcr %0, cr18" :: "r"(bugsr1));		\
	asm volatile ("stcr %0, cr19" :: "r"(bugsr2));		\
	asm volatile ("stcr %0, cr20" :: "r"(bugsr3));		\
}

#define	OSCTXT()						\
{								\
	asm volatile ("ldcr %0, cr17" : "=r" (bugsr0):: "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13");	\
	asm volatile ("ldcr %0, cr18" : "=r" (bugsr1));		\
	asm volatile ("ldcr %0, cr19" : "=r" (bugsr2));		\
	asm volatile ("ldcr %0, cr20" : "=r" (bugsr3));		\
								\
	asm volatile ("stcr %0, cr17" :: "r"(ossr0));		\
	asm volatile ("stcr %0, cr18" :: "r"(ossr1));		\
	asm volatile ("stcr %0, cr19" :: "r"(ossr2));		\
	asm volatile ("stcr %0, cr20" :: "r"(ossr3));		\
}

void
buginit()
{
	asm volatile ("ldcr %0, cr17" : "=r" (bugsr0));
	asm volatile ("ldcr %0, cr18" : "=r" (bugsr1));
	asm volatile ("ldcr %0, cr19" : "=r" (bugsr2));
	asm volatile ("ldcr %0, cr20" : "=r" (bugsr3));
}

char
buginchr(void)
{
	register int cc asm("r2");
	BUGCTXT();
	asm volatile ("or r9,r0," INCHR);
	asm volatile ("tb0 0,r0,0x1F0");
	asm volatile ("or %0,r0,r2" : "=r" (cc) : );
	OSCTXT();
	return ((char)cc & 0xFF);
}

bugoutchr(unsigned char c)
{
	unsigned char cc;

	if ((cc = c) == '\n') {
		bugpcrlf();
		return;
	}
	
	BUGCTXT();

	asm("or r2,r0,%0" : : "r" (cc));
	asm("or r9,r0," OUTCHR);
	asm("tb0 0,r0,0x1F0");

	OSCTXT();
}

/* return 1 if not empty else 0 */

buginstat(void)
{
	int ret;

	BUGCTXT();
	asm volatile ("or r9,r0," INSTAT);
	asm volatile ("tb0 0,r0,0x1F0");
	asm volatile ("or %0,r0,r2" : "=r" (ret) : );
	OSCTXT();
	return (ret & 0x4 ? 1 : 0);
}

bugoutstr(char *s, char *se)
{
	BUGCTXT();
	asm("or r9,r0," OUTSTR);
	asm("tb0 0,r0,0x1F0");
	OSCTXT();
}

bugpcrlf(void)
{
	BUGCTXT();
	asm("or r9,r0," PCRLF);
	asm("tb0 0,r0,0x1F0");
	OSCTXT();
}

/* return 0 on success */

bugdskrd(struct bugdisk_io *arg)
{
	int ret;

	BUGCTXT();
	asm("or r9,r0, " DSKRD);
	asm("tb0 0,r0,0x1F0");	
	asm("or %0,r0,r2" : "=r" (ret) : );
	OSCTXT();

	return ((ret&0x4) == 0x4 ? 1 : 0);
}

/* return 0 on success */

bugdskwr(struct bugdisk_io *arg)
{
	int ret;
	BUGCTXT();
	asm("or r9,r0, " DSKWR);
	asm("tb0 0,r0,0x1F0");	
	asm("or %0,r0,r2" : "=r" (ret) : );
	OSCTXT();
	return ((ret&0x4) == 0x4 ? 1 : 0);
}

bugrtcrd(struct bugrtc *rtc)
{
	BUGCTXT();
	asm("or r9,r0, " RTC_RD);
	asm("tb0 0,r0,0x1F0");
	OSCTXT();
}

bugreturn(void)
{
	BUGCTXT();
	asm("or r9,r0, " RETURN);
	asm("tb0 0,r0,0x1F0");
	OSCTXT();
}

bugbrdid(struct bugbrdid *id)
{
	struct bugbrdid *ptr;
	BUGCTXT();
	asm("or r9,r0, " BRD_ID);
	asm("tb0 0,r0,0x1F0");
	asm("or %0,r0,r2" : "=r" (ptr) : );
	OSCTXT();
	bcopy(ptr, id, sizeof(struct bugbrdid));
}

bugnetctrl(struct bugniocall *niocall)
{
/*	BUGCTXT();*/
	asm("or r2,r0,%0" : : "r" (niocall));
	asm("or r9,r0, " NETCTRL);
	asm("tb0 0,r0,0x1F0");
/*	OSCTXT();*/
}
