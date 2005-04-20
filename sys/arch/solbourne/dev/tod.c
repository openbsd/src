/*	$OpenBSD: tod.c,v 1.1 2005/04/20 01:00:16 miod Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODclock driver. We only use it to know the current time during boot,
 * as we do not get interrupts from it.
 *
 * The clock in the IDT machines is the Oki MSM62X42BRS.
 *
 * A datasheet for this chip is available from:
 *   http://www.datasheetarchive.com/datasheet/pdf/19/196099.html
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <solbourne/dev/todreg.h>
#include <dev/clock_subr.h>

#include <machine/idt.h>
#include <machine/kap.h>

int	todmatch(struct device *, void *, void *);
void	todattach(struct device *, struct device *, void *);

struct cfattach tod_ca = {
	sizeof(struct device), todmatch, todattach
};

struct cfdriver tod_cd = {
	NULL, "tod", DV_DULL
};

volatile u_char	*tod_regs;

u_char	msm_read(u_int);
void	msm_write(u_int, u_char);

int
todmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;

	return (strcmp(tod_cd.cd_name, ca->ca_ra.ra_name) == 0);
}

void
todattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	printf(": OKI MSM62X42BRS\n");

	/* the register are already mapped 1:1 by pmap_bootstrap() */
	tod_regs = (volatile u_char *)TODCLOCK_BASE;
}

/*
 * Read or write a register of the Oki clock.
 *
 * The clock registers are not directly accessible (while control registers
 * are). We need to freeze them first. To do so, we set the hold bit in
 * D, and if the busy bit clears, we are free to proceed. If the busy bit
 * is still set, we need to clear the hold bit and retry.
 */
u_char
msm_read(u_int regno)
{
	u_char d, r;

	/* no need to do the hold dance for control registers */
	if (regno >= MSM_D)
		return (tod_regs[regno] & 0x0f);

	d = tod_regs[MSM_D] & 0x0f & ~MSM_D_HOLD;
	for (;;) {
		tod_regs[MSM_D] = d | MSM_D_HOLD;
		if (!ISSET(tod_regs[MSM_D], MSM_D_BUSY))
			break;
		tod_regs[MSM_D] = d;
	}

	r = tod_regs[regno] & 0x0f;
	tod_regs[MSM_D] = d;

	return (r);
}

void
msm_write(u_int regno, u_char value)
{
	u_char d;

	/* no need to do the hold dance for control registers */
	if (regno >= MSM_D) {
		tod_regs[regno] = value;
		return;
	}

	d = tod_regs[MSM_D] & 0x0f & ~MSM_D_HOLD;
	for (;;) {
		tod_regs[MSM_D] = d | MSM_D_HOLD;
		if (!ISSET(tod_regs[MSM_D], MSM_D_BUSY))
			break;
		tod_regs[MSM_D] = d;
	}

	tod_regs[regno] = value;
	tod_regs[MSM_D] = d;
}

void
inittodr(base)
	time_t base;
{
	struct clock_ymdhms dt;

	dt.dt_sec = msm_read(MSM_SEC_UNITS) + 10 * msm_read(MSM_SEC_TENS);
	dt.dt_min = msm_read(MSM_MIN_UNITS) + 10 * msm_read(MSM_MIN_TENS);
#if 0
	dt.dt_hour = msm_read(MSM_HOUR_UNITS) + 10 * msm_read(MSM_HOUR_TENS);
#else
	dt.dt_hour = msm_read(MSM_HOUR_TENS);
	if (dt.dt_hour & MSM_HOUR_PM)
		dt.dt_hour = 12 + 10 * (dt.dt_hour & ~MSM_HOUR_TENS);
	else
		dt.dt_hour *= 10;
	dt.dt_hour += msm_read(MSM_HOUR_UNITS);
#endif
	dt.dt_day = msm_read(MSM_DAY_UNITS) + 10 * msm_read(MSM_DAY_TENS);
	dt.dt_mon = msm_read(MSM_MONTH_UNITS) + 10 * msm_read(MSM_MONTH_TENS);
	dt.dt_year = msm_read(MSM_YEAR_UNITS) + 10 * msm_read(MSM_YEAR_TENS);
	dt.dt_year += CLOCK_YEAR_BASE;
	/* dt_wday left uninitialized */

	time.tv_sec = clock_ymdhms_to_secs(&dt);

	if (time.tv_sec == 0) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the clock.
		 */
		if (base < 35 * SECYR) {/* this port did not exist until 2005 */
			/*
			 * If base is 0, assume filesystem time is just unknown
			 * in stead of preposterous. Don't bark.
			 */
			if (base != 0)
				printf("WARNING: preposterous time in file system\n");
			/* not going to use it anyway, if the chip is readable */
			time.tv_sec = 35 * SECYR + 90 * SECDAY + SECDAY / 2;
		} else {
			printf("WARNING: bad date in battery clock");
			time.tv_sec = base;
			resettodr();
		}
	} else {
		int deltat = time.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;
		
#ifndef SMALL_KERNEL
		printf("WARNING: clock %s %d days",
		    time.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
#endif
	}
	printf(" -- CHECK AND RESET THE DATE!\n");
}

void
resettodr()
{
	struct clock_ymdhms dt;

	if (time.tv_sec == 0 || tod_regs == NULL)
		return;

	clock_secs_to_ymdhms(time.tv_sec, &dt);

	/*
	 * Since we don't know if the clock is in AM/PM or 24 hour mode,
	 * we need to reset it and force one mode. Being an evil european
	 * person, I'll force 24 hour mode, of course.
	 */
	msm_write(MSM_F, MSM_F_RESET | MSM_F_24HR);
	msm_write(MSM_F, MSM_F_STOP);	/* leave reset mode, but stop clock */
	
	dt.dt_year -= CLOCK_YEAR_BASE;
	msm_write(MSM_YEAR_TENS, dt.dt_year / 10);
	msm_write(MSM_YEAR_UNITS, dt.dt_year % 10);
	msm_write(MSM_MONTH_TENS, dt.dt_mon / 10);
	msm_write(MSM_MONTH_UNITS, dt.dt_mon % 10);
	msm_write(MSM_DAY_TENS, dt.dt_day / 10);
	msm_write(MSM_DAY_UNITS, dt.dt_day % 10);
	msm_write(MSM_HOUR_TENS, dt.dt_hour / 10);
	msm_write(MSM_HOUR_UNITS, dt.dt_hour % 10);
	msm_write(MSM_MIN_TENS, dt.dt_min / 10);
	msm_write(MSM_MIN_UNITS, dt.dt_min % 10);
	msm_write(MSM_SEC_TENS, dt.dt_sec / 10);
	msm_write(MSM_SEC_UNITS, dt.dt_sec % 10);

	msm_write(MSM_F, 0);	/* restart clock */
}
