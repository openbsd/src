/* $NetBSD: rtc.c,v 1.1 1996/04/19 19:49:06 mark Exp $ */

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * rtc.c
 *
 * Routines to read and write the RTC and CMOS RAM
 *
 * Created      : 13/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <machine/iic.h>
#include <machine/rtc.h>

struct rtc_softc {
	struct device	sc_dev;
	int		sc_flags;
#define RTC_BROKEN	1
#define RTC_OPEN	2
};

void rtcattach __P((struct device *parent, struct device *self, void *aux));
int rtcmatch __P((struct device *parent, void *match, void *aux));

/* Read a byte from CMOS RAM */

int
cmos_read(location)
	int location;
{
	u_char buff;

/*
 * This commented code dates from when I was translating CMOS address
 * from the RISCOS addresses. Now all addresses are specifed as
 * actual addresses in the CMOS RAM
 */

/* 
 	if (location > 0xF0)
		return(-1);

	if (location < 0xC0)
		buff = location + 0x40;
	else
		buff = location - 0xB0;
*/
	buff = location;

	if (iic_control(RTC_Write, &buff, 1))
		return(-1);
	if (iic_control(RTC_Read, &buff, 1))
		return(-1);

	return(buff);
}


/* Write a byte to CMOS RAM */

int
cmos_write(location, value)
	int location;
	int value;
{
	u_char buff[2];

/*
 * This commented code dates from when I was translating CMOS address
 * from the RISCOS addresses. Now all addresses are specifed as
 * actual addresses in the CMOS RAM
 */

/*	if (location > 0xF0)
		return(-1);

	if (location < 0xC0)
		buff = location + 0x40;
	else
		buff = location - 0xB0;
*/
	buff[0] = location;
	buff[1] = value;

	if (iic_control(RTC_Write, buff, 2))
		return(-1);

	return(0);
}


/* Hex to BCD and BCD to hex conversion routines */

static inline int
hexdectodec(n)
	u_char n;
{
	return(((n >> 4) & 0x0F) * 10 + (n & 0x0F));
}

static inline int
dectohexdec(n)
	u_char n;
{
	return(((n / 10) << 4) + (n % 10));
}


/* Write the RTC data from an 8 byte buffer */

int
rtc_write(rtc)
	rtc_t *rtc;
{
	u_char buff[8];

	buff[0] = 1;

	buff[1] = dectohexdec(rtc->rtc_centi);
	buff[2] = dectohexdec(rtc->rtc_sec);
	buff[3] = dectohexdec(rtc->rtc_min);
	buff[4] = dectohexdec(rtc->rtc_hour) & 0x3f;
	buff[5] = dectohexdec(rtc->rtc_day);
	buff[6] = dectohexdec(rtc->rtc_mon);

	if (iic_control(RTC_Write, buff, 7))
		return(0);

	cmos_write(RTC_ADDR_YEAR, rtc->rtc_year);
	cmos_write(RTC_ADDR_CENT, rtc->rtc_cen);

	return(1);
}


/* Read the RTC data into a 8 byte buffer */

int
rtc_read(rtc)
	rtc_t *rtc;
{
	u_char buff[8];
	int byte;
    
	buff[0] = 0;

	if (iic_control(RTC_Write, buff, 1))
		return(0);

	if (iic_control(RTC_Read, buff, 8))
		return(0);

	rtc->rtc_micro = 0;
	rtc->rtc_centi = hexdectodec(buff[1] & 0xff);
	rtc->rtc_sec   = hexdectodec(buff[2] & 0x7f);
	rtc->rtc_min   = hexdectodec(buff[3] & 0x7f);
	rtc->rtc_hour  = hexdectodec(buff[4] & 0x3f);

	/* If in 12 hour mode need to look at the AM/PM flag */
	
	if (buff[4] & 0x80)
		rtc->rtc_hour += (buff[4] & 0x40) ? 12 : 0;

	rtc->rtc_day   = hexdectodec(buff[5] & 0x3f);
	rtc->rtc_mon   = hexdectodec(buff[6] & 0x1f);

	byte = cmos_read(RTC_ADDR_YEAR);
	if (byte == -1)
		return(0);
	rtc->rtc_year = byte; 

	byte = cmos_read(RTC_ADDR_CENT);
	if (byte == -1)
		return(0);
	rtc->rtc_cen = byte; 

	return(1);
}


struct cfattach rtc_ca = {
	sizeof(struct rtc_softc), rtcmatch, rtcattach
};

struct cfdriver rtc_cd = {
	NULL, "rtc", DV_DULL, 0
};

int
rtcmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
/*	struct iicbus_attach_args *ib = aux;*/

	return(1);
}


void
rtcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct rtc_softc *sc = (struct rtc_softc *)self;
	struct iicbus_attach_args *ib = aux;
	u_char buff[1];

	sc->sc_flags |= RTC_BROKEN;
	if ((ib->ib_addr & IIC_PCF8583_MASK) == IIC_PCF8583_ADDR) {
		printf(": PCF8583");

		buff[0] = 0;

		if (iic_control(RTC_Write, buff, 1))
			return;

		if (iic_control(RTC_Read, buff, 1))
			return;

		printf(" clock base ");
		switch (buff[0] & 0x30) {
		case 0x00:
			printf("32.768KHz");
			break;
		case 0x10:
			printf("50Hz");
			break;
		case 0x20:
			printf("event");
			break;
		case 0x30:
			printf("test mode");
			break;
		}

		if (buff[0] & 0x80)
			printf(" stopped");
		if (buff[0] & 0x04)
			printf(" alarm enabled");
		sc->sc_flags &= ~RTC_BROKEN;
	}


/*
 * Initialise the time of day register.
 * This is normally left to the filing system to do but not all
 * filing systems call it e.g. cd9660
 */

	inittodr(0);

	printf("\n");
}


int
rtcopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct rtc_softc *sc;
	int unit = minor(dev);
    
	if (unit >= rtc_cd.cd_ndevs)
		return(ENXIO);

	sc = rtc_cd.cd_devs[unit];
    
	if (!sc) return(ENXIO);

	if (sc->sc_flags & RTC_OPEN) return(EBUSY);

	sc->sc_flags |= RTC_OPEN;

	return(0);
}


int
rtcclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct rtc_softc *sc = rtc_cd.cd_devs[unit];
    
	sc->sc_flags &= ~RTC_OPEN;

	return(0);
}


int
rtcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = minor(dev);
	struct rtc_softc *sc = rtc_cd.cd_devs[unit];

	return(ENXIO);
}


int
rtcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = minor(dev);
	struct rtc_softc *sc = rtc_cd.cd_devs[unit];

	return(ENXIO);
}


int
rtcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct rtc_softc *sc = rtc_cd.cd_devs[minor(dev)];

/*	switch (cmd) {
	case RTCIOC_READ:
		return(0);
	}   */

	return(EINVAL);
}

/* End of rtc.c */
