/*	$OpenBSD: apm.c,v 1.23 1998/11/15 16:36:49 art Exp $	*/

/*-
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

#if defined(DEBUG) || defined(APMDEBUG)
#define DPRINTF(x)	printf x
#define STATIC /**/
#else
#define	DPRINTF(x)	/**/
#define STATIC static
#endif

int apmprobe __P((struct device *, void *, void *));
void apmattach __P((struct device *, struct device *, void *));

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

STATIC u_int apm_flags;
STATIC u_char apm_majver;
STATIC u_char apm_minver;
int apm_dobusy;
STATIC struct {
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

STATIC int apmcall __P((u_int, u_int, struct apmregs *));
STATIC void apm_power_print __P((struct apm_softc *, struct apmregs *));
STATIC void apm_event_handle __P((struct apm_softc *, struct apmregs *));
STATIC void apm_set_ver __P((struct apm_softc *));
STATIC void apm_periodic_check __P((void *));
/* STATIC void apm_disconnect __P((void *)); */
STATIC void apm_perror __P((const char *, struct apmregs *));
/* STATIC void apm_powmgt_enable __P((int onoff)); */
STATIC void apm_powmgt_engage __P((int onoff, u_int devid));
/* STATIC void apm_devpowmgt_enable __P((int onoff, u_int devid)); */
STATIC int apm_record_event __P((struct apm_softc *sc, u_int event_type));
STATIC const char *apm_err_translate __P((int code));

#define	apm_get_powstat(r) apmcall(APM_POWER_STATUS, APM_DEV_ALLDEVS, &r)
#define	apm_get_event(r) apmcall(APM_GET_PM_EVENT, 0, &r)
#define	apm_suspend() apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_SUSPEND)
#define	apm_standby() apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_STANDBY)

STATIC const char *
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

STATIC void
apm_perror(str, regs)
	const char *str;
	struct apmregs *regs;
{
	printf("apm0: APM %s: %s (%d)\n", str,
	    apm_err_translate(APM_ERR_CODE(regs)),
	    APM_ERR_CODE(regs));

	apmerrors++;
}

STATIC void
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
		printf(" battery charge ");
		switch (BATT_STATE(regs)) {
		case APM_BATT_HIGH:
			printf("high");
			break;
		case APM_BATT_LOW:
			printf("low");
			break;
		case APM_BATT_CRITICAL:
			printf("critical");
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

/*
 * 	call the APM protected mode bios function FUNCTION for BIOS selection
 * 	WHICHBIOS.
 *	Fills in *regs with registers as returned by APM.
 *	returns nonzero if error returned by APM.
 */
STATIC int
apmcall(f, dev, r)
	u_int f, dev;
	struct apmregs *r;
{
	register int rv;

	/* todo: do something with %edi? */
	__asm __volatile(
#if defined(DEBUG) || defined(DIAGNOSTIC)
			"pushl %%ds; pushl %%es; pushl %%fs; pushl %%gs\n\t"
			"pushfl; cli\n\t"
			"xorl	%0, %0\n\t"
			"movl	%0, %%ds\n\t"
			"movl	%0, %%es\n\t"
			"movl	%0, %%fs\n\t"
			"movl	%0, %%gs\n\t"
#endif
			"movl	%5, %%eax\n\t"
			"lcall	%%cs:(%9)\n\t"
			"pushl %1; setc %b1; movzbl %b1, %0; popl %1\n\t"

#if defined(DEBUG) || defined(DIAGNOSTIC)
			"popfl; popl %%gs; popl %%fs; popl %%es; popl %%ds"
#endif
			: "=r" (rv),
			  "=a" (r->ax), "=b" (r->bx), "=c" (r->cx), "=d" (r->dx)
			: "m" (f), "2" (dev), "3" (r->cx), "4" (r->dx),
			  "m" (apm_ep)
			: "cc", "memory");
	return rv;
}


int apm_standbys = 0;
int apm_userstandbys = 0;
int apm_suspends = 0;
int apm_battlow = 0;

static int apm_evindex = 0;

STATIC int
apm_record_event(sc, event_type)
	struct apm_softc *sc;
	u_int event_type;
{
	struct apm_event_info *evp;

	if ((sc->sc_flags & SCFLAG_OPEN) == 0)
		return 1;		/* no user waiting */
	if (sc->event_count == APM_NEVENTS)
		return 1;			/* overflow */
	evp = &sc->event_list[sc->event_ptr];
	sc->event_count++;
	sc->event_ptr++;
	sc->event_ptr %= APM_NEVENTS;
	evp->type = event_type;
	evp->index = ++apm_evindex;
	selwakeup(&sc->sc_rsel);
	return (sc->sc_flags & SCFLAG_OWRITE) ? 0 : 1; /* user may handle */
}

STATIC void
apm_event_handle(sc, regs)
	struct apm_softc *sc;
	struct apmregs *regs;
{
	int error;
	struct apmregs nregs;

	switch(regs->bx) {
	case APM_USER_STANDBY_REQ:
		DPRINTF(("user wants STANDBY--fat chance\n"));
		(void) apm_set_powstate(APM_DEV_ALLDEVS, APM_LASTREQ_REJECTED);
		if (apm_record_event(sc, regs->bx))
			apm_userstandbys++;
		break;
	case APM_STANDBY_REQ:
		DPRINTF(("standby requested\n"));
		if (apm_standbys || apm_suspends)
			DPRINTF(("damn fool BIOS did not wait for answer\n"));
		if (apm_record_event(sc, regs->bx)) {
			(void) apm_set_powstate(APM_DEV_ALLDEVS,
			    APM_LASTREQ_INPROG);
			apm_standbys++;
		} else
			(void) apm_set_powstate(APM_DEV_ALLDEVS,
			    APM_LASTREQ_REJECTED);
		break;
	case APM_USER_SUSPEND_REQ:
		DPRINTF(("user wants suspend--fat chance!\n"));
		(void) apm_set_powstate(APM_DEV_ALLDEVS, APM_LASTREQ_REJECTED);
		if (apm_record_event(sc, regs->bx))
			apm_suspends++;
		break;
	case APM_SUSPEND_REQ:
		DPRINTF(("suspend requested\n"));
		if (apm_standbys || apm_suspends)
			DPRINTF(("damn fool BIOS did not wait for answer\n"));
		if (apm_record_event(sc, regs->bx)) {
			(void) apm_set_powstate(APM_DEV_ALLDEVS,
			    APM_LASTREQ_INPROG);
			apm_suspends++;
		} else
			(void) apm_set_powstate(APM_DEV_ALLDEVS,
			    APM_LASTREQ_REJECTED);
		break;
	case APM_POWER_CHANGE:
		DPRINTF(("power status change\n"));
		error = apm_get_powstat(nregs);
		if (error == 0 &&
		    (sc->sc_flags & SCFLAG_PRINT) != SCFLAG_NOPRINT &&
		    ((sc->sc_flags & SCFLAG_PRINT) != SCFLAG_PCTPRINT ||
		     sc->batt_life != BATT_LIFE(&nregs)))
			apm_power_print(sc, &nregs);
		apm_record_event(sc, regs->bx);
		break;
	case APM_NORMAL_RESUME:
		DPRINTF(("system resumed\n"));
		inittodr(time.tv_sec);
		apm_record_event(sc, regs->bx);
		break;
	case APM_CRIT_RESUME:
		DPRINTF(("system resumed without us!\n"));
		inittodr(time.tv_sec);
		apm_record_event(sc, regs->bx);
		break;
	case APM_SYS_STANDBY_RESUME:
		DPRINTF(("system standby resume\n"));
		inittodr(time.tv_sec);
		apm_record_event(sc, regs->bx);
		break;
	case APM_UPDATE_TIME:
		DPRINTF(("update time, please\n"));
		inittodr(time.tv_sec);
		apm_record_event(sc, regs->bx);
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
	default:
		printf("APM nonstandard event code %x\n", regs->bx);
	}
}

STATIC void
apm_periodic_check(arg)
	void *arg;
{
	register struct apm_softc *sc = arg;
	struct apmregs regs;

	while (apm_get_event(regs) == 0)
		apm_event_handle(sc, &regs);

	if (APM_ERR_CODE(&regs) != APM_ERR_NOEVENTS)
		apm_perror("get event", &regs);
	if (apm_suspends /*|| (apm_battlow && apm_userstandbys)*/) {
		/* stupid TI TM5000! */
		apm_suspend();
	} else if (apm_standbys || apm_userstandbys) {
		apm_standby();
	}
	apm_suspends = apm_standbys = apm_battlow = apm_userstandbys = 0;

	if(apmerrors < 10)
		timeout(apm_periodic_check, sc, hz);
#ifdef DIAGNOSTIC
	else
		printf("APM: too many errors, turning off timeout\n");
#endif
}

#ifdef notused
STATIC void
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
#endif

STATIC void
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
STATIC void
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

#ifdef APM_V10_ONLY
int apm_v11_enabled = 0;
#else
int apm_v11_enabled = 1;
#endif

STATIC void
apm_set_ver(self)
	struct apm_softc *self;
{
	struct apmregs regs;
	int error;

	bzero(&regs, sizeof(regs));
	regs.cx = 0x0101;	/* APM Version 1.1 */
	
	if (apm_v11_enabled && (error =
	    apmcall(APM_DRIVER_VERSION, APM_DEV_APM_BIOS, &regs)) == 0) {
		apm_majver = APM_CONN_MAJOR(&regs);
		apm_minver = APM_CONN_MINOR(&regs);
	} else {
		apm_majver = 1;
		apm_minver = 0;
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
STATIC void
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
	    ba->bios_apmp->apm_detail & APM_BIOS_PM_DISABLED ||
	    !(ba->bios_apmp->apm_detail & APM_32BIT_SUPPORTED)) {
		DPRINTF(("%s: %x\n", ba->bios_dev, ba->bios_apmp->apm_detail));
		return 0;
	}

	if (ap->apm_code32_base + ap->apm_code_len > IOM_END)
		ap->apm_code_len -= ap->apm_code32_base + ap->apm_code_len -
		    IOM_END;
	if (bus_space_map(ba->bios_memt, ap->apm_code32_base,
	    ap->apm_code_len, 1, &ch) != 0) {
		DPRINTF(("apm0: can't map code\n"));
		return 0;
	}
	bus_space_unmap(ba->bios_memt, ch, ap->apm_code_len);
	if (ap->apm_data_base + ap->apm_data_len > IOM_END)
	    ap->apm_data_len -= ap->apm_data_base + ap->apm_data_len - IOM_END;
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
	bus_space_handle_t ch, dh;

	/*
	 * set up GDT descriptors for APM
	 */
	if (ap->apm_detail & APM_32BIT_SUPPORTED) {
		apm_flags = ap->apm_detail;
		apm_ep.seg = GSEL(GAPM32CODE_SEL,SEL_KPL);
		apm_ep.entry = ap->apm_entry;
		if ((ap->apm_code32_base <= ap->apm_data_base &&
		     ap->apm_code32_base+ap->apm_code_len >= ap->apm_data_base)
		  ||(ap->apm_code32_base >= ap->apm_data_base &&
		     ap->apm_data_base+ap->apm_data_len>=ap->apm_code32_base)){
			int l;
			l = max(ap->apm_data_base + ap->apm_data_len,
				ap->apm_code32_base + ap->apm_data_len) -
			    min(ap->apm_data_base, ap->apm_code32_base);
			bus_space_map(ba->bios_memt,
				min(ap->apm_data_base, ap->apm_code32_base),
				l, 1, &ch);
			dh = ch;
			if (ap->apm_data_base < ap->apm_code32_base)
				ch += ap->apm_code32_base - ap->apm_data_base;
			else
				dh += ap->apm_data_base - ap->apm_code32_base;
		} else {

			bus_space_map(ba->bios_memt,
				      ba->bios_apmp->apm_code32_base,
				      ba->bios_apmp->apm_code_len, 1, &ch);
			bus_space_map(ba->bios_memt,
				      ba->bios_apmp->apm_data_base,
				      ba->bios_apmp->apm_data_len, 1, &dh);
		}
		setsegment(&dynamic_gdt[GAPM32CODE_SEL].sd, (void *)ch,
			   ap->apm_code_len-1, SDT_MEMERA, SEL_KPL, 1, 0);
		setsegment(&dynamic_gdt[GAPM16CODE_SEL].sd, (void *)ch,
			   ap->apm_code_len-1, SDT_MEMERA, SEL_KPL, 0, 0);
		setsegment(&dynamic_gdt[GAPMDATA_SEL].sd, (void *)dh,
			   ap->apm_data_len-1, SDT_MEMRWA, SEL_KPL, 1, 0);
		DPRINTF((": flags %x code 32:%x/%x 16:%x/%x %x "
		       "data %x/%x/%x ep %x (%x:%x)\n%s", apm_flags,
		    ap->apm_code32_base, ch, ap->apm_code16_base, ch,
		    ap->apm_code_len, ap->apm_data_base, dh, ap->apm_data_len,
		    ap->apm_entry, apm_ep.seg, ap->apm_entry+ch,
		    sc->sc_dev.dv_xname));
		apm_set_ver(sc);
		/*
		 * Engage cooperative power mgt (we get to do it)
		 * on all devices (v1.1).
		 */
		apm_powmgt_engage(1, APM_DEV_ALLDEVS);
#if 0
		/* doesn't seem to work, sigh. */
		apm_powmgt_engage(1, APM_DEV_DISPLAY(APM_DEV_ALLUNITS));
		apm_powmgt_engage(1, APM_DEV_DISK(APM_DEV_ALLUNITS));
		apm_powmgt_engage(1, APM_DEV_PARALLEL(APM_DEV_ALLUNITS));
		apm_powmgt_engage(1, APM_DEV_NETWORK(APM_DEV_ALLUNITS));
		apm_powmgt_engage(1, APM_DEV_PCMCIA(APM_DEV_ALLUNITS));
#endif
		
		if (apm_get_powstat(regs) == 0) {
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
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 || !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
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
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 || !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
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
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 || !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
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
		if (apm_get_powstat(regs) == 0) {
			bzero(powerp, sizeof(*powerp));
			if (BATT_LIFE(&regs) != APM_BATT_LIFE_UNKNOWN)
				powerp->battery_life = BATT_LIFE(&regs);
			powerp->ac_state = AC_STATE(&regs);
			switch (apm_minver) {
			case 0:
				powerp->battery_state = BATT_STATE(&regs);
				break;
			case 1:
			default:
				powerp->battery_state = APM_BATT_UNKNOWN;
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
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 || !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
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
