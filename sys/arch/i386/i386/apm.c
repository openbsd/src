/*	$NetBSD $	*/

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
#if NAPM > 0

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

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/psl.h>

#include <dev/isa/isareg.h>
#include <i386/isa/isa_machdep.h>
#include <i386/isa/nvram.h>
#include <dev/isa/isavar.h>

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
	int	event_count;
	int	event_ptr;
	struct	apm_event_info event_list[APM_NEVENTS];
};
#define	SCFLAG_OREAD	0x0000001
#define	SCFLAG_OWRITE	0x0000002
#define	SCFLAG_OPEN	(SCFLAG_OREAD|SCFLAG_OWRITE)

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

struct apm_connect_info apminfo = { 0 };
u_char apm_majver;
u_char apm_minver;
u_short	apminited;
int apm_dobusy;

STATIC int apm_get_powstat __P((struct apmregs *));
STATIC void apm_power_print __P((struct apm_softc *, struct apmregs *));
STATIC void apm_event_handle __P((struct apm_softc *, struct apmregs *));
STATIC int apm_get_event __P((struct apmregs *));
STATIC void apm_set_ver __P((struct apm_softc *));
STATIC void apm_periodic_check __P((void *));
STATIC void apm_disconnect __P((void *));
STATIC void apm_perror __P((const char *, struct apmregs *));
STATIC void apm_powmgt_enable __P((int onoff));
STATIC void apm_powmgt_engage __P((int onoff, u_int devid));
STATIC void apm_devpowmgt_enable __P((int onoff, u_int devid));
STATIC void apm_suspend __P((void));
STATIC void apm_standby __P((void));
STATIC int apm_record_event __P((struct apm_softc *sc, u_int event_type));

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

STATIC void
apm_perror(str, regs)
const char *str;
struct apmregs *regs;
{
	printf("APM %s: %s (%d)\n", str,
	       apm_err_translate(APM_ERR_CODE(regs)),
	       APM_ERR_CODE(regs));
}

STATIC void
apm_power_print (sc, regs)
struct apm_softc *sc;
struct apmregs *regs;
{
	if (BATT_LIFE(regs) != APM_BATT_LIFE_UNKNOWN) {
		printf("%s: battery life expectancy %d%%\n",
		       sc->sc_dev.dv_xname,
		       BATT_LIFE(regs));
	}
	printf("%s: A/C state: ", sc->sc_dev.dv_xname);
	switch (AC_STATE(regs)) {
	case APM_AC_OFF:
		printf("off\n");
		break;
	case APM_AC_ON:
		printf("on\n");
		break;
	case APM_AC_BACKUP:
		printf("backup power\n");
		break;
	default:
	case APM_AC_UNKNOWN:
		printf("unknown\n");
		break;
	}
	printf("%s: battery charge state:", sc->sc_dev.dv_xname);
	if (apm_minver == 0)
		switch (BATT_STATE(regs)) {
		case APM_BATT_HIGH:
			printf("high\n");
			break;
		case APM_BATT_LOW:
			printf("low\n");
			break;
		case APM_BATT_CRITICAL:
			printf("critical\n");
			break;
		case APM_BATT_CHARGING:
			printf("charging\n");
			break;
		case APM_BATT_UNKNOWN:
			printf("unknown\n");
			break;
		default:
			printf("undecoded state %x\n", BATT_STATE(regs));
			break;
		}
	else if (apm_minver >= 1) {
		if (BATT_FLAGS(regs) & APM_BATT_FLAG_NOBATTERY)
			printf(" no battery");
		else {
			if (BATT_FLAGS(regs) & APM_BATT_FLAG_HIGH)
				printf(" high");
			if (BATT_FLAGS(regs) & APM_BATT_FLAG_LOW)
				printf(" low");
			if (BATT_FLAGS(regs) & APM_BATT_FLAG_CRITICAL)
				printf(" critical");
			if (BATT_FLAGS(regs) & APM_BATT_FLAG_CHARGING)
				printf(" charging");
		}
		printf("\n");
		if (BATT_REM_VALID(regs))
			printf("%s: estimated %d:%02d minutes\n",
			       sc->sc_dev.dv_xname,
			       BATT_REMAINING(regs) / 60,
			       BATT_REMAINING(regs)%60);
	}
	return;
}

int apm_standbys = 0;
int apm_userstandbys = 0;
int apm_suspends = 0;
int apm_battlow = 0;

STATIC void
apm_get_powstate(dev)
u_int dev;
{
    struct apmregs regs;
    int rval;
    regs.bx = dev;
    rval = apmcall(APM_GET_POWER_STATE, &regs);
    if (rval == 0) {
	printf("apm dev %04x state %04x\n", dev, regs.cx);
    }
}

STATIC void
apm_suspend()
{
	(void) apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_SUSPEND);
}

STATIC void
apm_standby()
{
	(void) apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_STANDBY);
}

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
		(void) apm_record_event(sc, regs->bx);
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
		(void) apm_set_powstate(APM_DEV_ALLDEVS,
					APM_LASTREQ_REJECTED);
		apm_suspends++;
		apm_record_event(sc, regs->bx);
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
		error = apm_get_powstat(&nregs);
		if (error == 0)
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

STATIC int
apm_get_event(regs)
struct apmregs *regs;
{
    return apmcall(APM_GET_PM_EVENT, regs);
}

STATIC void
apm_periodic_check(arg)
void *arg;
{
	struct apmregs regs;
	struct apm_softc *sc = arg;
	while (apm_get_event(&regs) == 0) {
		apm_event_handle(sc, &regs);
	};
	if (APM_ERR_CODE(&regs) != APM_ERR_NOEVENTS)
		apm_perror("get event", &regs);
	if (apm_suspends /*|| (apm_battlow && apm_userstandbys)*/) {
		/* stupid TI TM5000! */
		apm_suspend();
	} else if (apm_standbys || apm_userstandbys) {
		apm_standby();
	}
	apm_suspends = apm_standbys = apm_battlow = apm_userstandbys =0;
	timeout(apm_periodic_check, sc, hz);
}

STATIC void
apm_powmgt_enable(onoff)
int onoff;
{
	struct apmregs regs;
	regs.bx = apm_minver == 0 ? APM_MGT_ALL : APM_DEV_APM_BIOS;
	regs.cx = onoff ? APM_MGT_ENABLE : APM_MGT_DISABLE;
	if (apmcall(APM_PWR_MGT_ENABLE, &regs) != 0)
		apm_perror("power management enable", &regs);
}

STATIC void
apm_powmgt_engage(onoff, dev)
int onoff;
u_int dev;
{
	struct apmregs regs;
	if (apm_minver == 0)
		return;
	regs.bx = dev;
	regs.cx = onoff ? APM_MGT_ENGAGE : APM_MGT_DISENGAGE;
	if (apmcall(APM_PWR_MGT_ENGAGE, &regs) != 0)
		printf("APM power mgmt engage (device %x): %s (%d)\n",
		       dev, apm_err_translate(APM_ERR_CODE(&regs)),
		       APM_ERR_CODE(&regs));
}

STATIC void
apm_devpowmgt_enable(onoff, dev)
int onoff;
u_int dev;
{
	struct apmregs regs;
	if (apm_minver == 0)
	    return;
	regs.bx = dev;
	/* enable is auto BIOS managment.
	 * disable is program control.
	 */
	regs.cx = onoff ? APM_MGT_ENABLE : APM_MGT_DISABLE;
	if (apmcall(APM_DEVICE_MGMT_ENABLE, &regs) != 0)
		printf("APM device engage (device %x): %s (%d)\n",
		       dev, apm_err_translate(APM_ERR_CODE(&regs)),
		       APM_ERR_CODE(&regs));
}

int
apm_set_powstate(dev, state)
u_int dev, state;
{
	struct apmregs regs;
	if (!apminited || (apm_minver == 0 && state > APM_SYS_OFF))
	    return EINVAL;
	regs.bx = dev;
	regs.cx = state;
	if (apmcall(APM_SET_PWR_STATE, &regs) != 0) {
		apm_perror("set power state", &regs);
		return EIO;
	}
	return 0;
}

#ifdef APM_NOIDLE
int apmidleon = 0;
#else
int apmidleon = 1;
#endif

void
apm_cpu_busy()
{
	struct apmregs regs;
	if (!apminited)
	    return;
	if ((apminfo.apm_detail & APM_IDLE_SLOWS) &&
	    apmcall(APM_CPU_BUSY, &regs) != 0)
		apm_perror("set CPU busy", &regs);
}

void
apm_cpu_idle()
{
	struct apmregs regs;
	if (!apminited || !apmidleon)
	    return;
	if (apmcall(APM_CPU_IDLE, &regs) != 0)
		apm_perror("set CPU idle", &regs);
}

void *apm_sh;

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

	regs.cx = 0x0101;	/* APM Version 1.1 */
	regs.bx = APM_DEV_APM_BIOS;
	
	if (apm_v11_enabled &&
	    (error = apmcall(APM_DRIVER_VERSION, &regs)) == 0) {
		apm_majver = APM_CONN_MAJOR(&regs);
		apm_minver = APM_CONN_MINOR(&regs);
	} else {
		apm_majver = 1;
		apm_minver = 0;
	}
	printf(": Power Management spec V%d.%d",
	       apm_majver, apm_minver);
	apminited = 1;
	if (apminfo.apm_detail & APM_IDLE_SLOWS) {
#ifdef DEBUG
	/* not relevant much */
		printf(" (slowidle)");
#endif
		apm_dobusy = 1;
	} else
	    apm_dobusy = 0;
#ifdef DIAGNOSTIC
	if (apminfo.apm_detail & APM_BIOS_PM_DISABLED)
		printf(" (BIOS mgmt disabled)");
	if (apminfo.apm_detail & APM_BIOS_PM_DISENGAGED)
		printf(" (BIOS managing devices)");
#endif
	printf("\n");
}

STATIC int
apm_get_powstat(regs)
struct apmregs *regs;
{
	regs->bx = APM_DEV_ALLDEVS;
	return apmcall(APM_POWER_STATUS, regs);
}

STATIC void
apm_disconnect(xxx)
void *xxx;
{
	struct apmregs regs;
	regs.bx = apm_minver == 1 ? APM_DEV_ALLDEVS : APM_DEFAULTS_ALL;
	if (apmcall(APM_SYSTEM_DEFAULTS, &regs))
		apm_perror("system defaults failed", &regs);

	regs.bx = APM_DEV_APM_BIOS;
	if (apmcall(APM_DISCONNECT, &regs))
		apm_perror("disconnect failed", &regs);
	else
		printf("APM disconnected\n");
}

int
apmprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct apm_attach_args *aaa = aux;

	if (apminited)
		return 0;
	if ((apminfo.apm_detail & APM_32BIT_SUPPORTED) &&
	    strcmp(aaa->aaa_busname, "apm") == 0) {
		return 1;
	}
	return 0;
}

void
apmattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct apm_softc *apmsc = (void *)self;
	struct apmregs regs;
	int error;
	/*
	 * set up GDT descriptors for APM
	 */
	if (apminfo.apm_detail & APM_32BIT_SUPPORTED) {
		apminfo.apm_segsel = GSEL(GAPM32CODE_SEL,SEL_KPL);
		apminfo.apm_code32_seg_base <<= 4;
		apminfo.apm_code16_seg_base <<= 4;
		apminfo.apm_data_seg_base <<= 4;
		/* something is still amiss in the limit-fetch in the boot
		   loader; it returns incorrect (too small) limits.
		   for now, force them to max size. */
		apminfo.apm_code32_seg_len = 65536;
		apminfo.apm_data_seg_len = 65536;
#if 0
		switch ((APM_MAJOR_VERS(apminfo.apm_detail) << 8) +
			APM_MINOR_VERS(apminfo.apm_detail)) {
		case 0x0100:
			apminfo.apm_code32_seg_len = 65536;
			apminfo.apm_data_seg_len = 65536;
			break;
		default:
			if (apminfo.apm_data_seg_len == 0)
				apminfo.apm_data_seg_len = 65536;
			break;
		}
#endif
		setsegment(&dynamic_gdt[GAPM32CODE_SEL].sd,
			   (void *)ISA_HOLE_VADDR(apminfo.apm_code32_seg_base),
			   apminfo.apm_code32_seg_len-1,
			   SDT_MEMERA, SEL_KPL, 1, 0);
		setsegment(&dynamic_gdt[GAPM16CODE_SEL].sd,
			   (void *)ISA_HOLE_VADDR(apminfo.apm_code16_seg_base),
			   65536-1,	/* just in case */
			   SDT_MEMERA, SEL_KPL, 0, 0);
		setsegment(&dynamic_gdt[GAPMDATA_SEL].sd,
			   (void *)ISA_HOLE_VADDR(apminfo.apm_data_seg_base),
			   apminfo.apm_data_seg_len-1,
			   SDT_MEMRWA, SEL_KPL, 1, 0);
#if defined(DEBUG) || defined(APMDEBUG)
		printf(": detail %x 32b:%x/%x/%x 16b:%x/%x data %x/%x/%x ep %x (%x:%x)\n%s",
		       apminfo.apm_detail,
		       apminfo.apm_code32_seg_base,
		       ISA_HOLE_VADDR(apminfo.apm_code32_seg_base),
		       apminfo.apm_code32_seg_len,
		       apminfo.apm_code16_seg_base,
		       ISA_HOLE_VADDR(apminfo.apm_code16_seg_base),
		       apminfo.apm_data_seg_base,
		       ISA_HOLE_VADDR(apminfo.apm_data_seg_base),
		       apminfo.apm_data_seg_len,
		       apminfo.apm_entrypt,
		       apminfo.apm_segsel,
		       apminfo.apm_entrypt+ISA_HOLE_VADDR(apminfo.apm_code32_seg_base),
		       apmsc->sc_dev.dv_xname);
#endif
		apm_set_ver(apmsc);
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
		error = apm_get_powstat(&regs);
		if (error == 0) {
			apm_power_print(apmsc, &regs);
		} else
			apm_perror("get power status", &regs);
		apm_cpu_busy();
		apm_periodic_check(apmsc);
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
	int unit = APMUNIT(dev);
	int ctl = APMDEV(dev);
	struct apm_softc *sc;

	if (unit >= apm_cd.cd_ndevs)
		return ENXIO;
	sc = apm_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;
	
	switch (ctl) {
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
	struct apm_softc *sc = apm_cd.cd_devs[APMUNIT(dev)];
	int ctl = APMDEV(dev);

	DPRINTF(("apmclose: pid %d flag %x mode %x\n", p->p_pid, flag, mode));
	switch (ctl) {
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
	int error;
	struct apm_softc *sc = apm_cd.cd_devs[APMUNIT(dev)];
	struct apm_power_info *powerp;
	struct apm_event_info *evp;
	struct apmregs regs;
	register int i;
	struct apm_ctl *actl;

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
	case APM_IOC_DEV_CTL:
		actl = (struct apm_ctl *)data;
		if ((flag & FWRITE) == 0)
			return EBADF;
		apm_get_powstate(actl->dev); /* XXX */
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
		error = apm_get_powstat(&regs);
		if (error == 0) {
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
			error = EIO;
		}
		break;
		
	default:
		return ENOTTY;
	}
	return 0;
}

int
apmselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	struct apm_softc *sc = apm_cd.cd_devs[APMUNIT(dev)];

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

#endif /* NAPM > 0 */
