#include "bugio.h"

#define INCHR	"0x0000"
#define INSTAT	"0x0001"
#define INLN	"0x0002"
#define READSTR	"0x0003"
#define READLN	"0x0004"
#define	DSKRD	"0x0010"
#define	DSKWR	"0x0011"
#define	DSKCFIG	"0x0012"
#define	OUTCHR	"0x0020"
#define	PCRLF	"0x0026"
#define	TMDISP	"0x0042"
#define	DELAY	"0x0043"
#define	RTC_DSP	"0x0052"
#define	RTC_RD	"0x0053"
#define	RETURN	"0x0063"
#define	BRD_ID	"0x0070"
#define BUGTRAP	"0x01F0"

char
buginchr(void)
{
	register int cc asm("r2");
	asm("or r9,r0," INCHR);
	asm("tb0 0,r0,0x1F0");
	/*asm("or %0,r0,r2" : "=r" (cc) : );*/
	return ((char)cc & 0xFF);
}

/* return 1 if not empty else 0 */

buginstat(void)
{
	int ret;
	asm("or r9,r0," INSTAT);
	asm("tb0 0,r0,0x1F0");
	asm("or %0,r0,r2" : "=r" (ret) : );
	return (ret & 0x40 ? 1 : 0);
}

bugoutchr(unsigned char c)
{
	unsigned char cc;

	if ((cc = c) == '\n') {
		bugpcrlf();
		return;
	}
	asm("or r2,r0,%0" : : "r" (cc));
	asm("or r9,r0," OUTCHR);
	asm("tb0 0,r0,0x1F0");
}

bugpcrlf(void)
{
	asm("or r9,r0," PCRLF);
	asm("tb0 0,r0,0x1F0");
}
/* return 0 on success */

bugdskrd(struct bugdisk_io *arg)
{
	int ret;
	asm("or r9,r0, " DSKRD);
	asm("tb0 0,r0,0x1F0");	
	asm("or %0,r0,r2" : "=r" (ret) : );
	return ((ret&0x4) == 0x4 ? 1 : 0);
}

/* return 0 on success */

bugdskwr(struct bugdisk_io *arg)
{
	int ret;
	asm("or r9,r0, " DSKWR);
	asm("tb0 0,r0,0x1F0");	
	asm("or %0,r0,r2" : "=r" (ret) : );
	return ((ret&0x4) == 0x4 ? 1 : 0);
}

bugrtcrd(struct bugrtc *rtc)
{
	asm("or r9,r0, " RTC_RD);
	asm("tb0 0,r0,0x1F0");
}

bugreturn(void)
{
	asm("or r9,r0, " RETURN);
	asm("tb0 0,r0,0x1F0");
}

bugbrdid(struct bugbrdid *id)
{
	struct bugbrdid *ptr;
	asm("or r9,r0, " BRD_ID);
	asm("tb0 0,r0,0x1F0");
	asm("or %0,r0,r2" : "=r" (ptr) : );
	bcopy(ptr, id, sizeof(struct bugbrdid));
}
