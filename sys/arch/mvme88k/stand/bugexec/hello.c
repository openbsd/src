#include "bug.h"
#include "bugio.h"

void	putchar	__P((char));
int	bcd2int	__P((unsigned int));

void
putchar(char c)
{
	bugoutchr(c);
}

main(struct bugenv *env)
{
	struct bugrtc rtc;
	struct bugbrdid brdid;

	bugrtcrd(&rtc);
	printf("From RTC:\n");
	printf("Year %d\tMonth %d\tDay %d\tDay of Week %d\n",
		bcd2int(rtc.Y), bcd2int(rtc.M), bcd2int(rtc.D), bcd2int(rtc.d));
	printf("Hour %d\tMin %d\tSec %d\tCal %d\n",
		bcd2int(rtc.H), bcd2int(rtc.m), bcd2int(rtc.s), bcd2int(rtc.c));
	printf("From BRDID:\n");
	bugbrdid(&brdid);
/*	printf("Eye catcher %c%c%c%c\n", brdid.eye[0], brdid.eye[1],
			brdid.eye[2], brdid.eye[3]); */
	printf("Board no %d (%d) \tsuffix %c%c\n", bcd2int(brdid.brdno),
		 brdid.brdno, brdid.brdsuf[0], brdid.brdsuf[1]);
/*	printf("Clun %x\tdlun %x\n", brdid.clun, brdid.dlun); */
	return 0;
}

ipow(int base, int i)
{
	int cnt = 1;
	while (i--) {
		cnt *= base;
	}
	return cnt;	
}

int
bcd2int(unsigned int i)
{
	unsigned val = 0;
	int	cnt = 0;
	while (i) {
		val += (i&0xf) * ipow(10,cnt);
		cnt++;
		i >>= 4;
	}
	return val;
}
