/*	$OpenBSD: apm.c,v 1.29 2000/01/31 02:04:35 mickey Exp $	*/

/*-
 * Copyright (c) 1998-2000 Michael Shalayeff. All rights reserved.
 * Copyright (c) 1995 John T. Kohl.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "apm.h"

#if NAPM > 1
#error only one APM device may be configured
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/psl.h>

#include <dev/isa/isareg.h>
#include <i386/isa/isa_machdep.h>
#include <i386/isa/nvram.h>
#include <dev/isa/isavar.h>

#include <machine/biosvar.h>
#include <machine/apmvar.h>

#if defined(APMDEBUG)
#define DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	/**/
#endif

int apmprobe __P((struct device *, void *, void *));
void apmattach __P((struct device *, struct device *, void *));

/* battery percentage at where we get verbose in our warnings.  This
   value can be changed using sysctl(8), value machdep.apmwarn.
   Setting it to zero kills all warnings */
int cpu_apmwarn = 10;

#define APM_NEVENTS 16

struct apm_softc {
	struct device sc_dev;
	struct selinfo sc_rsel;
	struct selinfo sc_xsel;
	int	sc_flags;
	int	batt_life;
	int	event_count;
	int	event_ptr;
	struct	apm_event_info event_list[APM_NEVENTS];
};
#define	SCFLAG_OREAD	0x0000001
#define	SCFLAG_OWRITE	0x0000002
#define	SCFLAG_OPEN	(SCFLAG_OREAD|SCFLAG_OWRITE)

/*
 * Flags to control kernel display
 * 	SCFLAG_NOPRINT:		do not output APM power messages due to
 *				a power change event.
 *
 *	SCFLAG_PCTPRINT:	do not output APM power messages due to
 *				to a power change event unless the battery
 *				percentage changes.
 */
#define SCFLAG_NOPRINT	0x0008000
#define SCFLAG_PCTPRINT	0x0004000
#define SCFLAG_PRINT	(SCFLAG_NOPRINT|SCFLAG_PCTPRINT)

#define	APMUNIT(dev)	(minor(dev)&0xf0)
#define	APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

struct cfattach apm_ca = {
	sizeof(struct apm_softc), apmprobe, apmattach
};

struct cfdriver apm_cd = {
	NULL, "apm", DV_DULL
};

int apm_standbys;
int apm_userstandbys;
int apm_suspends;
int apm_battlow;
int apm_evindex;
int apm_error;
int apm_op_inprog;

u_int apm_flags;
u_char apm_majver;
u_char apm_minver;
int apm_dobusy;
struct {
	u_int32_t entry;
	u_int16_t seg;
	u_int16_t pad;
} apm_ep;

struct apmregs {
	u_int32_t	ax;
	u_int32_t	bx;
	u_int32_t	cx;
	u_int32_t	dx;
};

int apmcall __P((u_int, u_int, struct apmregs *));
void apm_power_print __P((struct apm_softc *, struct apmregs *));
void apm_event_handle __P((struct apm_softc *, struct apmregs *));
void apm_set_ver __P((struct apm_softc *));
void apm_periodic_check __P((void *));
/* void apm_disconnect __P((void *)); */
void apm_perror __P((const char *, struct apmregs *));
void apm_powmgt_enable __P((int onoff));
void apm_powmgt_engage __P((int onoff, u_int devid));
/* void apm_devpowmgt_enable __P((int onoff, u_int devid)); */
int apm_record_event __P((struct apm_softc *sc, u_int event_type));
const char *apm_err_translate __P((int code));

#define	apm_get_powstat(r) apmcall(APM_POWER_STATUS, APM_DEV_ALLDEVS, r)
void	apm_standby __P((void));
void	apm_suspend __P((void));
void	apm_resume __P((struct apm_softc *, struct apmregs *));

static int __inline
apm_get_event(struct apmregs *r)
{
	int rv;
	rv = apmcall(APM_GET_PM_EVENT, 0, r);
#ifdef APMDEBUG
	if (r->bx)
		printf("apm_get_event: %x\n", r->bx);
#endif
	return rv;
}

const char *
apm_err_translate(code)
	int code;
{
	switch(code) {
	case APM_ERR_PM_DISABLED:
		return "power management disabled";
	case APM_ERR_REALALREADY:
		return "real mode interface already connected";
	case APM_ERR_NOTCONN:
		return "interface not connected";
	case APM_ERR_16ALREADY:
		return "16-bit interface already connected";
	case APM_ERR_16NOTSUPP:
		return "16-bit interface not supported";
	case APM_ERR_32ALREADY:
		return "32-bit interface already connected";
	case APM_ERR_32NOTSUPP:
		return "32-bit interface not supported";
	case APM_ERR_UNRECOG_DEV:
		return "unrecognized device ID";
	case APM_ERR_ERANGE:
		return "parameter out of range";
	case APM_ERR_NOTENGAGED:
		return "interface not engaged";
	case APM_ERR_UNABLE:
		return "unable to enter requested state";
	case APM_ERR_NOEVENTS:
		return "No pending events";
	case APM_ERR_NOT_PRESENT:
		return "No APM present";
	default:
		return "unknown error code?";
	}
}

int apmerrors = 0;

void
apm_perror(str, regs)
	const char *str;
	struct apmregs *regs;
{
	printf("apm0: APM %s: %s (%d)\n", str,
	    apm_err_translate(APM_ERR_CODE(regs)),
	    APM_ERR_CODE(regs));

	apmerrors++;
}

void
apm_power_print (sc, regs)
	struct apm_softc *sc;
	struct apmregs *regs;
{
#if !defined(APM_NOPRINT)
	sc->batt_life = BATT_LIFE(regs);
	if (BATT_LIFE(regs) != APM_BATT_LIFE_UNKNOWN) {
		printf("%s: battery life expectancy %d%%\n",
		    sc->sc_dev.dv_xname,
		    BATT_LIFE(regs));
	}
	printf("%s: AC ", sc->sc_dev.dv_xname);
	switch (AC_STATE(regs)) {
	case APM_AC_OFF:
		printf("off,");
		break;
	case APM_AC_ON:
		printf("on,");
		break;
	case APM_AC_BACKUP:
		printf("backup power,");
		break;
	default:
	case APM_AC_UNKNOWN:
		printf("unknown,");
		break;
	}
	if (apm_minver == 0) {
		printf(" battery is ");
		switch (BATT_STATE(regs)) {
		case APM_BATT_HIGH:
			printf("high");
			break;
		case APM_BATT_LOW:
			printf("low");
			break;
		case APM_BATT_CRITICAL:
			printf("CRITICAL");
			break;
		case APM_BATT_CHARGING:
			printf("charging");
			break;
		case APM_BATT_UNKNOWN:
			printf("unknown");
			break;
		default:
			printf("undecoded (%x)", BATT_STATE(regs));
			break;
		}
	} else if (apm_minver >= 1) {
		if (BATT_FLAGS(regs) & APM_BATT_FLAG_NOBATTERY)
			printf(" no battery");
		else {
			printf(" battery charge ");
			if (BATT_FLAGS(regs) & APM_BATT_FLAG_HIGH)
				printf("high");
			else if (BATT_FLAGS(regs) & APM_BATT_FLAG_LOW)
				printf("low");
			else if (BATT_FLAGS(regs) & APM_BATT_FLAG_CRITICAL)
				printf("critical");
			else
				printf("unknown");
			if (BATT_FLAGS(regs) & APM_BATT_FLAG_CHARGING)
				printf(", charging");
			if (BATT_REM_VALID(regs))
				printf(", estimated %d:%02d minutes",
				    BATT_REMAINING(regs) / 60,
				    BATT_REMAINING(regs) % 60);
		}
	}

	printf("\n");
#endif
}

void
apm_suspend()
{
	dopowerhooks(PWR_SUSPEND);

	(void)apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_SUSPEND);
}

void
apm_standby()
{
	dopowerhooks(PWR_STANDBY);

	(void)apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_STANDBY);
}

void
apm_resume(sc, regs)
	struct apm_softc *sc;
	struct apmregs *regs;
{
	/* they say that some machines may require reinititalizing the clock */
	initrtclock();

	inittodr(time.tv_sec);
	dopowerhooks(PWR_RESUME);
	apm_record_event(sc, regs->bx);
}

/*
 * 	call the APM protected mode bios function FUNCTION for BIOS selection
 * 	WHICHBIOS.
 *	Fills in *regs with registers as returned by APM.
 *	returns nonzero if error returned by APM.
 */
int
apmcall(f, dev, r)
	u_int f, dev;
	struct apmregs *r;
{
	register int rv;

	/* todo: do something with %edi? */
	__asm __volatile(
#if defined(DEBUG) || defined(DIAGNOSTIC)
			"pushl %%ds; pushl %%es; pushl %%fs; pushl %%gs\n\t"
			"pushfl\n\t"
			"xorl	%0, %0\n\t"
			"movl	%0, %%ds\n\t"
			"movl	%0, %%es\n\t"
			"movl	%0, %%fs\n\t"
			"movl	%0, %%gs\n\t"
#endif
			"movl	%5, %%eax\n\t"
			"clc\n\t"
			"sti\n\t"
			"lcall	%%cs:(%7)\n\t"
			"pushl %1; setc %b1; movzbl %b1, %0; popl %1\n\t"

#if defined(DEBUG) || defined(DIAGNOSTIC)
			"popfl; popl %%gs; popl %%fs; popl %%es; popl %%ds"
#endif
			: "=r" (rv),
			  "=a" (r->ax), "=b" (r->bx), "+c" (r->cx), "+d" (r->dx)
			: "m" (f), "2" (dev), "m" (apm_ep)
			: "edi", "ebp", "cc", "memory");
	return rv;
}


int
apm_record_event(sc, event_type)
	struct apm_softc *sc;
	u_int event_type;
{
	struct apm_event_info *evp;

	if (!apm_error && (sc->sc_flags & SCFLAG_OPEN) == 0) {
		DPRINTF(("apm_record_event: no user waiting\n"));
		apm_error++;
		return 1;		/* no user waiting */
	}
	if (!apm_error && sc->event_count == APM_NEVENTS) {
		DPRINTF(("apm_record_event: overflow\n"));
		apm_error++;
		return 1;			/* overflow */
	}
	evp = &sc->event_list[sc->event_ptr];
	sc->event_count++;
	sc->event_ptr++;
	sc->event_ptr %= APM_NEVENTS;
	evp->type = event_type;
	evp->index = ++apm_evindex;
	selwakeup(&sc->sc_rsel);
	return (sc->sc_flags & SCFLAG_OWRITE) ? 0 : 1; /* user may handle */
}

void
apm_event_handle(sc, regs)
	struct apm_softc *sc;
	struct apmregs *regs;
{
	int error;
	struct apmregs nregs;
	char *p;

	switch(regs->bx) {
	case APM_USER_STANDBY_REQ:
		DPRINTF(("user wants STANDBY--fat chance\n"));
		if (apm_record_event(sc, regs->bx))
			apm_userstandbys++;
		apm_set_powstate(APM_DEV_ALLDEVS, APM_LASTREQ_INPROG);
		apm_op_inprog++;
		break;
	case APM_STANDBY_REQ:
		DPRINTF(("standby requested\n"));
		if (apm_standbys || apm_suspends) {
			DPRINTF(("premature standby\n"));
			apm_error++;
		}
		if (apm_record_event(sc, regs->bx))
			apm_standbys++;
		apm_set_powstate(APM_DEV_ALLDEVS, APM_LASTREQ_INPROG);
		apm_op_inprog++;
		break;
	case APM_USER_SUSPEND_REQ:
		DPRINTF(("user wants suspend--fat chance!\n"));
		if (apm_record_event(sc, regs->bx))
			apm_suspends++;
		apm_set_powstate(APM_DEV_ALLDEVS, APM_LASTREQ_INPROG);
		apm_op_inprog++;
		break;
	case APM_SUSPEND_REQ:
		DPRINTF(("suspend requested\n"));
		if (apm_standbys || apm_suspends) {
			DPRINTF(("premature suspend\n"));
			apm_error++;
		}
		if (apm_record_event(sc, regs->bx))
			apm_suspends++;
		apm_set_powstate(APM_DEV_ALLDEVS, APM_LASTREQ_INPROG);
		apm_op_inprog++;
		break;
	case APM_POWER_CHANGE:
		DPRINTF(("power status change\n"));
		error = apm_get_powstat(&nregs);
		if (error == 0 &&
		    BATT_LIFE(&nregs) != APM_BATT_LIFE_UNKNOWN &&
		    BATT_LIFE(&nregs) < cpu_apmwarn &&
		    (sc->sc_flags & SCFLAG_PRINT) != SCFLAG_NOPRINT &&
		    ((sc->sc_flags & SCFLAG_PRINT) != SCFLAG_PCTPRINT ||
		     sc->batt_life != BATT_LIFE(&nregs)))
			apm_power_print(sc, &nregs);
		apm_record_event(sc, regs->bx);
		break;
	case APM_NORMAL_RESUME:
		DPRINTF(("system resumed\n"));
		apm_resume(sc, regs);
		break;
	case APM_CRIT_RESUME:
		DPRINTF(("system resumed without us!\n"));
		apm_resume(sc, regs);
		break;
	case APM_SYS_STANDBY_RESUME:
		DPRINTF(("system standby resume\n"));
		apm_resume(sc, regs);
		break;
	case APM_UPDATE_TIME:
		DPRINTF(("update time, please\n"));
		apm_resume(sc, regs);
		break;
	case APM_CRIT_SUSPEND_REQ:
		DPRINTF(("suspend required immediately\n"));
		apm_record_event(sc, regs->bx);
		apm_suspend();
		break;
	case APM_BATTERY_LOW:
		DPRINTF(("Battery low!\n"));
		apm_battlow++;
		apm_record_event(sc, regs->bx);
		break;
	case APM_CAPABILITY_CHANGE:
		DPRINTF(("capability change\n"));
		if (apm_minver < 2) {
			DPRINTF(("adult event\n"));
		} else {
			if (apmcall(APM_GET_CAPABILITIES, APM_DEV_APM_BIOS,
			    &nregs) != 0) {
				apm_perror("get capabilities", &nregs);
			} else {
				apm_get_powstat(&nregs);
			}
		}
		break;
	default:
		switch (regs->bx >> 8) {
		case 0:	p = "reserved system";	break;
		case 1:	p = "reserved device";	break;
		case 2:	p = "OEM defined";	break;
		default:p = "reserved";		break;
		}
		DPRINTF(("apm_handle_event: %s event, code %d\n", p, regs->bx));
	}
}

void
apm_periodic_check(arg)
	void *arg;
{
	register struct apm_softc *sc = arg;
	struct apmregs regs;

	if (apm_op_inprog)
		apm_set_powstate(APM_DEV_ALLDEVS, APM_LASTREQ_INPROG);

	while (apm_get_event(&regs) == 0 && !apm_error)
		apm_event_handle(sc, &regs);

	if (APM_ERR_CODE(&regs) != APM_ERR_NOEVENTS)
		apm_perror("get event", &regs);

	if (apm_suspends /*|| (apm_battlow && apm_userstandbys)*/) {
		apm_op_inprog = 0;
		/* stupid TI TM5000! */
		apm_suspend();
	} else if (apm_standbys || apm_userstandbys) {
		apm_op_inprog = 0;
		apm_standby();
	}
	apm_suspends = apm_standbys = apm_battlow = apm_userstandbys = 0;
	apm_error = 0;

	timeout(apm_periodic_check, sc, hz);
}

void
apm_powmgt_enable(onoff)
	int onoff;
{
	struct apmregs regs;
	bzero(&regs, sizeof(regs));
	regs.cx = onoff ? APM_MGT_ENABLE : APM_MGT_DISABLE;
	if (apmcall(APM_PWR_MGT_ENABLE,
	    (apm_minver? APM_DEV_APM_BIOS : APM_MGT_ALL), &regs) != 0)
		apm_perror("power management enable", &regs);
}

void
apm_powmgt_engage(onoff, dev)
	int onoff;
	u_int dev;
{
	struct apmregs regs;
	if (apm_minver == 0)
		return;
	bzero(&regs, sizeof(regs));
	regs.cx = onoff ? APM_MGT_ENGAGE : APM_MGT_DISENGAGE;
	if (apmcall(APM_PWR_MGT_ENGAGE, dev, &regs) != 0)
		printf("apm0: APM engage (device %x): %s (%d)\n",
		    dev, apm_err_translate(APM_ERR_CODE(&regs)),
		    APM_ERR_CODE(&regs));
}

#ifdef notused
void
apm_devpowmgt_enable(onoff, dev)
	int onoff;
	u_int dev;
{
	struct apmregs regs;
	if (apm_minver == 0)
		return;
	/* enable is auto BIOS managment.
	 * disable is program control.
	 */
	bzero(&regs, sizeof(regs));
	regs.cx = onoff ? APM_MGT_ENABLE : APM_MGT_DISABLE;
	if (apmcall(APM_DEVICE_MGMT_ENABLE, dev, &regs) != 0)
		printf("APM device engage (device %x): %s (%d)\n",
		    dev, apm_err_translate(APM_ERR_CODE(&regs)),
		    APM_ERR_CODE(&regs));
}
#endif

int
apm_set_powstate(dev, state)
	u_int dev, state;
{
	struct apmregs regs;
	if (!apm_cd.cd_ndevs || (apm_minver == 0 && state > APM_SYS_OFF))
		return EINVAL;
	bzero(&regs, sizeof(regs));
	regs.cx = state;
	if (apmcall(APM_SET_PWR_STATE, dev, &regs) != 0) {
		apm_perror("set power state", &regs);
		if (APM_ERR_CODE(&regs) == APM_ERR_UNRECOG_DEV)
			return ENXIO;
		else
			return EIO;
	}
	return 0;
}

int apmidleon = 1;

void
apm_cpu_busy()
{
	struct apmregs regs;
	if (!apm_cd.cd_ndevs || !apmidleon)
		return;
	bzero(&regs, sizeof(regs));
	if ((apm_flags & APM_IDLE_SLOWS) &&
		apmcall(APM_CPU_BUSY, 0, &regs) != 0) {

#ifdef DIAGNOSTIC
		apm_perror("set CPU busy", &regs);
#endif
		apmidleon = 0;
	}
}

void
apm_cpu_idle()
{
	struct apmregs regs;
	if (!apm_cd.cd_ndevs || !apmidleon)
		return;

	bzero(&regs, sizeof(regs));
	if (apmcall(APM_CPU_IDLE, 0, &regs) != 0) {

#ifdef DIAGNOSTIC
		apm_perror("set CPU idle", &regs);
#endif
		apmidleon = 0;
	}
}

void
apm_set_ver(self)
	struct apm_softc *self;
{
	struct apmregs regs;
	int rv;

	bzero(&regs, sizeof(regs));
	regs.cx = APM_VERSION;
	
	if (APM_MAJOR(apm_flags) == 1 && APM_MINOR(apm_flags) == 2 &&
	    (rv = apmcall(APM_DRIVER_VERSION, APM_DEV_APM_BIOS, &regs)) == 0) {
		apm_majver = APM_CONN_MAJOR(&regs);
		apm_minver = APM_CONN_MINOR(&regs);
	} else {
#ifdef APMDEBUG
		if (rv)
			apm_perror("set version 1.2", &regs);
#endif
		/* try downgrading to 1.1 */
		bzero(&regs, sizeof(regs));
		regs.cx = 0x0101;

	    	if (apmcall(APM_DRIVER_VERSION, APM_DEV_APM_BIOS, &regs) == 0) {
			apm_majver = 1;
			apm_minver = 1;
		} else {
#ifdef APMDEBUG
			apm_perror("set version 1.1", &regs);
#endif
			apm_majver = 1;
			apm_minver = 0;
		}
	}
	printf(": Power Management spec V%d.%d", apm_majver, apm_minver);
	if (apm_flags & APM_IDLE_SLOWS) {
		/* not relevant much */
		DPRINTF((" (slowidle)"));
		apm_dobusy = 1;
	} else
		apm_dobusy = 0;
#ifdef DIAGNOSTIC
	if (apm_flags & APM_BIOS_PM_DISABLED)
		printf(" (BIOS mgmt disabled)");
	if (apm_flags & APM_BIOS_PM_DISENGAGED)
		printf(" (BIOS managing devices)");
#endif
	printf("\n");
}

#ifdef notused
void
apm_disconnect(xxx)
	void *xxx;
{
	struct apmregs regs;
	bzero(&regs, sizeof(regs));
	if (apmcall(APM_SYSTEM_DEFAULTS,
	    (apm_minver == 1 ? APM_DEV_ALLDEVS : APM_DEFAULTS_ALL), &regs))
		apm_perror("system defaults failed", &regs);

	if (apmcall(APM_DISCONNECT, APM_DEV_APM_BIOS, &regs))
		apm_perror("disconnect failed", &regs);
	else
		printf("APM disconnected\n");
}
#endif

int
apmprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct bios_attach_args *ba = aux;
	bios_apminfo_t *ap = ba->bios_apmp;
	bus_space_handle_t ch, dh;

	if (apm_cd.cd_ndevs ||
	    strcmp(ba->bios_dev, "apm") ||
	    !(ba->bios_apmp->apm_detail & APM_32BIT_SUPPORTED)) {
		DPRINTF(("%s: %x\n", ba->bios_dev, ba->bios_apmp->apm_detail));
		return 0;
	}

	/* addresses check
	   since pc* console and vga* probes much later
	   we cannot check for video memory being mapped
	   for apm stuff w/ bus_space_map() */
	if ((ap->apm_code32_base < IOM_BEGIN &&
	     ap->apm_code32_base + ap->apm_code_len > IOM_BEGIN) ||
	    (ap->apm_code16_base < IOM_BEGIN &&
	     ap->apm_code16_base + ap->apm_code16_len > IOM_BEGIN) ||
	    (ap->apm_data_base < IOM_BEGIN &&
	     ap->apm_data_base + ap->apm_data_len > IOM_BEGIN))
		return 0;

	if (bus_space_map(ba->bios_memt, ap->apm_code32_base,
	    ap->apm_code_len, 1, &ch) != 0) {
		DPRINTF(("apm0: can't map code\n"));
		return 0;
	}
	bus_space_unmap(ba->bios_memt, ch, ap->apm_code_len);

	if (bus_space_map(ba->bios_memt, ap->apm_data_base,
	    ap->apm_data_len, 1, &dh) != 0) {
		DPRINTF(("apm0: can't map data\n"));
		return 0;
	}
	bus_space_unmap(ba->bios_memt, dh, ap->apm_data_len);

	return 1;
}

void
apmattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	extern union descriptor *dynamic_gdt;
	struct bios_attach_args *ba = aux;
	bios_apminfo_t *ap = ba->bios_apmp;
	struct apm_softc *sc = (void *)self;
	struct apmregs regs;
	u_int cbase, clen, l;
	bus_space_handle_t ch16, ch32, dh;

	/*
	 * set up GDT descriptors for APM
	 */
	if (ap->apm_detail & APM_32BIT_SUPPORTED) {

		/* adjust code size limits */
		if (ap->apm_code_len >= 0x10000)
			ap->apm_code_len = 0xffff;
		if (ap->apm_code16_len >= 0x10000)
			ap->apm_code16_len = 0xffff;

		apm_flags = ap->apm_detail;
		apm_ep.seg = GSEL(GAPM32CODE_SEL,SEL_KPL);
		apm_ep.entry = ap->apm_entry;
		cbase = min(ap->apm_code32_base, ap->apm_code16_base);
		clen = max(ap->apm_code32_base + ap->apm_code_len,
			   ap->apm_code16_base + ap->apm_code16_len) - cbase;
		if ((cbase <= ap->apm_data_base &&
		     cbase + clen >= ap->apm_data_base) ||
		    (ap->apm_data_base <= cbase &&
		     ap->apm_data_base + ap->apm_data_len >= cbase)) {
			l = max(ap->apm_data_base + ap->apm_data_len + 1,
				cbase + clen + 1) -
			    min(ap->apm_data_base, cbase);
			bus_space_map(ba->bios_memt,
				min(ap->apm_data_base, cbase),
				l, 1, &dh);
			ch16 = dh;
			if (ap->apm_data_base < cbase)
				ch16 += cbase - ap->apm_data_base;
			else
				dh += ap->apm_data_base - cbase;
		} else {

			bus_space_map(ba->bios_memt, cbase, clen + 1, 1, &ch16);
			bus_space_map(ba->bios_memt, ap->apm_data_base,
				      ap->apm_data_len + 1, 1, &dh);
		}
		ch32 = ch16;
		if (ap->apm_code16_base == cbase)
			ch32 += ap->apm_code32_base - cbase;
		else
			ch16 += ap->apm_code16_base - cbase;

		setsegment(&dynamic_gdt[GAPM32CODE_SEL].sd, (void *)ch32,
			   ap->apm_code_len, SDT_MEMERA, SEL_KPL, 1, 0);
		setsegment(&dynamic_gdt[GAPM16CODE_SEL].sd, (void *)ch16,
			   ap->apm_code16_len, SDT_MEMERA, SEL_KPL, 0, 0);
		setsegment(&dynamic_gdt[GAPMDATA_SEL].sd, (void *)dh,
			   ap->apm_data_len, SDT_MEMRWA, SEL_KPL, 1, 0);
		DPRINTF((": flags %x code 32:%x/%x[%x] 16:%x/%x[%x] "
		       "data %x/%x/%x ep %x (%x:%x)\n%s", apm_flags,
		    ap->apm_code32_base, ch32, ap->apm_code_len,
		    ap->apm_code16_base, ch16, ap->apm_code16_len,
		    ap->apm_data_base, dh, ap->apm_data_len,
		    ap->apm_entry, apm_ep.seg, ap->apm_entry+ch32,
		    sc->sc_dev.dv_xname));

		if (ap->apm_detail & APM_BIOS_PM_DISABLED)
			apm_powmgt_enable(1);
		/*
		 * Engage cooperative power mgt (we get to do it)
		 * on all devices (v1.1).
		 */
		apm_powmgt_engage(1, APM_DEV_ALLDEVS);

		apm_set_ver(sc);

		bzero(&regs, sizeof(regs));
		if (apm_get_powstat(&regs) == 0) {
			apm_power_print(sc, &regs);
		} else
			apm_perror("get power status", &regs);
		apm_cpu_busy();
		apm_periodic_check(sc);
	} else {
		dynamic_gdt[GAPM32CODE_SEL] = dynamic_gdt[GNULL_SEL];
		dynamic_gdt[GAPM16CODE_SEL] = dynamic_gdt[GNULL_SEL];
		dynamic_gdt[GAPMDATA_SEL] = dynamic_gdt[GNULL_SEL];
	}
}

int
apmopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct apm_softc *sc;

	/* apm0 only */
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
		return ENXIO;
	
	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		if (!(flag & FWRITE))
			return EINVAL;
		if (sc->sc_flags & SCFLAG_OWRITE)
			return EBUSY;
		sc->sc_flags |= SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		if (!(flag & FREAD) || (flag & FWRITE))
			return EINVAL;
		sc->sc_flags |= SCFLAG_OREAD;
		break;
	default:
		return ENXIO;
		break;
	}
	return 0;
}

int
apmclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct apm_softc *sc;

	/* apm0 only */
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
		return ENXIO;
	
	DPRINTF(("apmclose: pid %d flag %x mode %x\n", p->p_pid, flag, mode));

	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		sc->sc_flags &= ~SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		sc->sc_flags &= ~SCFLAG_OREAD;
		break;
	}
	if ((sc->sc_flags & SCFLAG_OPEN) == 0) {
		sc->event_count = 0;
		sc->event_ptr = 0;
	}
	return 0;
}

int
apmioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct apm_softc *sc;
	struct apm_power_info *powerp;
	struct apm_event_info *evp;
	struct apmregs regs;
	register int i;
	struct apm_ctl *actl;

	/* apm0 only */
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
		return ENXIO;
	
	switch (cmd) {
		/* some ioctl names from linux */
	case APM_IOC_STANDBY:
		if ((flag & FWRITE) == 0)
			return EBADF;
		apm_userstandbys++;
		return 0;
	case APM_IOC_SUSPEND:
		if ((flag & FWRITE) == 0)
			return EBADF;
		apm_suspends++;
		return 0;
	case APM_IOC_PRN_CTL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		{
			int flag = *(int*)data;
			DPRINTF(( "APM_IOC_PRN_CTL: %d\n", flag ));
			switch (flag) {
			case APM_PRINT_ON:	/* enable printing */
				sc->sc_flags &= ~SCFLAG_PRINT;
				return 0;
			case APM_PRINT_OFF: /* disable printing */
				sc->sc_flags &= ~SCFLAG_PRINT;
				sc->sc_flags |= SCFLAG_NOPRINT;
				return 0;
			case APM_PRINT_PCT: /* disable some printing */
				sc->sc_flags &= ~SCFLAG_PRINT;
				sc->sc_flags |= SCFLAG_PCTPRINT;
				return 0;
			default:
				break;
			}
		}
		return EINVAL;
	case APM_IOC_DEV_CTL:
		actl = (struct apm_ctl *)data;
		if ((flag & FWRITE) == 0)
			return EBADF;
		{
			struct apmregs regs;
			
			bzero(&regs, sizeof(regs));
			if (!apmcall(APM_GET_POWER_STATE, actl->dev, &regs))
				printf("%s: dev %04x state %04x\n",
				       sc->sc_dev.dv_xname, dev, regs.cx);
		}
		return apm_set_powstate(actl->dev, actl->mode);
	case APM_IOC_NEXTEVENT:
		if (sc->event_count) {
			evp = (struct apm_event_info *)data;
			i = sc->event_ptr + APM_NEVENTS - sc->event_count;
			i %= APM_NEVENTS;
			*evp = sc->event_list[i];
			sc->event_count--;
			return 0;
		} else
			return EAGAIN;
	case APM_IOC_GETPOWER:
		powerp = (struct apm_power_info *)data;
		if (apm_get_powstat(&regs) == 0) {
			bzero(powerp, sizeof(*powerp));
			if (BATT_LIFE(&regs) != APM_BATT_LIFE_UNKNOWN)
				powerp->battery_life = BATT_LIFE(&regs);
			powerp->ac_state = AC_STATE(&regs);
			switch (apm_minver) {
			case 0:
				if (!(BATT_FLAGS(&regs) & APM_BATT_FLAG_NOBATTERY))
					powerp->battery_state = BATT_STATE(&regs);
				break;
			case 1:
			default:
				if (BATT_FLAGS(&regs) & APM_BATT_FLAG_HIGH)
					powerp->battery_state = APM_BATT_HIGH;
				else if (BATT_FLAGS(&regs) & APM_BATT_FLAG_LOW)
					powerp->battery_state = APM_BATT_LOW;
				else if (BATT_FLAGS(&regs) & APM_BATT_FLAG_CRITICAL)
					powerp->battery_state = APM_BATT_CRITICAL;
				else if (BATT_FLAGS(&regs) & APM_BATT_FLAG_CHARGING)
					powerp->battery_state = APM_BATT_CHARGING;
				else if (BATT_FLAGS(&regs) & APM_BATT_FLAG_NOBATTERY)
					powerp->battery_state = APM_BATTERY_ABSENT;
				else
					powerp->battery_state = APM_BATT_UNKNOWN;
				if (BATT_REM_VALID(&regs))
					powerp->minutes_left = BATT_REMAINING(&regs);
			}
		} else {
			apm_perror("ioctl get power status", &regs);
			return (EIO);
		}
		break;
		
	default:
		return (ENOTTY);
	}

	return 0;
}

int
apmselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	struct apm_softc *sc;

	/* apm0 only */
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
		return ENXIO;
	
	switch (rw) {
	case FREAD:
		if (sc->event_count)
			return 1;
		selrecord(p, &sc->sc_rsel);
		break;
	case FWRITE:
	case 0:
		return 0;
	}
	return 0;
}
