/*	$OpenBSD: pxa2x0_apm.c,v 1.2 2005/01/20 23:34:36 uwe Exp $	*/

/*-
 * Copyright (c) 2001 Alexander Guy.  All rights reserved.
 * Copyright (c) 1998-2001 Michael Shalayeff. All rights reserved.
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
 * OR SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mount.h>		/* for vfs_syncwait() */
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/event.h>

#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/apmvar.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_apm.h>

#if defined(APMDEBUG)
#define DPRINTF(x)	printf x
#else
#define	DPRINTF(x)	/**/
#endif

#define APM_LOCK(sc)    lockmgr(&(sc)->sc_lock, LK_EXCLUSIVE, NULL, curproc)
#define APM_UNLOCK(sc)  lockmgr(&(sc)->sc_lock, LK_RELEASE, NULL, curproc)

struct cfdriver apm_cd = {
	NULL, "apm", DV_DULL
};

#define	APMUNIT(dev)	(minor(dev)&0xf0)
#define	APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

int	apm_userstandbys;
int	apm_suspends;
int	apm_battlow;

void	apm_battery_info(struct pxa2x0_apm_softc *,
    struct pxaapm_battery_info *);
void	apm_standby(struct pxa2x0_apm_softc *);
void	apm_suspend(struct pxa2x0_apm_softc *);
void	apm_resume(struct pxa2x0_apm_softc *);
void	apm_periodic_check(struct pxa2x0_apm_softc *);
void	apm_thread_create(void *);
void	apm_thread(void *);

void	pxa2x0_apm_standby(struct pxa2x0_apm_softc *);
void	pxa2x0_apm_sleep(struct pxa2x0_apm_softc *);

void	filt_apmrdetach(struct knote *kn);
int	filt_apmread(struct knote *kn, long hint);
int	apmkqfilter(dev_t dev, struct knote *kn);

struct filterops apmread_filtops =
	{ 1, NULL, filt_apmrdetach, filt_apmread};

/*
 * Flags to control kernel display
 *	SCFLAG_NOPRINT:		do not output APM power messages due to
 *				a power change event.
 *
 *	SCFLAG_PCTPRINT:	do not output APM power messages due to
 *				to a power change event unless the battery
 *				percentage changes.
 */

#define SCFLAG_NOPRINT	0x0008000
#define SCFLAG_PCTPRINT	0x0004000
#define SCFLAG_PRINT	(SCFLAG_NOPRINT|SCFLAG_PCTPRINT)

#define	SCFLAG_OREAD 	(1 << 0)
#define	SCFLAG_OWRITE	(1 << 1)
#define	SCFLAG_OPEN	(SCFLAG_OREAD|SCFLAG_OWRITE)

void
apm_battery_info(struct pxa2x0_apm_softc *sc,
    struct pxaapm_battery_info *battp)
{

	bzero(battp, sizeof(struct pxaapm_battery_info));
	if (sc->sc_battery_info != NULL)
		sc->sc_battery_info(battp);
}

void
apm_standby(struct pxa2x0_apm_softc *sc)
{

	dopowerhooks(PWR_STANDBY);

	if (cold)
		vfs_syncwait(0);

	pxa2x0_apm_standby((struct pxa2x0_apm_softc *)sc);
}

void
apm_suspend(struct pxa2x0_apm_softc *sc)
{

#if 0
	/* XXX something is not restored in PWR_RESUME. */
	dopowerhooks(PWR_SUSPEND);
#else
	dopowerhooks(PWR_STANDBY);
#endif

	if (cold)
		vfs_syncwait(0);

	pxa2x0_apm_sleep((struct pxa2x0_apm_softc *)sc);
}

void
apm_resume(struct pxa2x0_apm_softc *sc)
{

	dopowerhooks(PWR_RESUME);
}

void
apm_periodic_check(struct pxa2x0_apm_softc *sc)
{

	if (sc->sc_periodic_check != NULL)
		sc->sc_periodic_check(sc);

	if (apm_suspends) {
		apm_userstandbys = 0;
		apm_suspends = 0;
		apm_suspend(sc);
		apm_resume(sc);
	} else if (apm_userstandbys) {
		apm_userstandbys = 0;
		apm_standby(sc);
		apm_resume(sc);
	}
}

void
apm_thread_create(void *v)
{
	struct pxa2x0_apm_softc *sc = v;

	if (kthread_create(apm_thread, sc, &sc->sc_thread,
	    "%s", sc->sc_dev.dv_xname)) {
		/* apm_disconnect(sc); */
		printf("%s: failed to create kernel thread, disabled",
		    sc->sc_dev.dv_xname);
	}
}

void
apm_thread(void *v)
{
	struct pxa2x0_apm_softc *sc = v;

	for (;;) {
		APM_LOCK(sc);
		apm_periodic_check(sc);
		APM_UNLOCK(sc);
		tsleep(&lbolt, PWAIT, "apmev", 0);
	}
}

int
apmopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct pxa2x0_apm_softc *sc;
	int error = 0;

	/* apm0 only */
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
		return ENXIO;

	DPRINTF(("apmopen: dev %d pid %d flag %x mode %x\n",
	    APMDEV(dev), p->p_pid, flag, mode));

	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		if (!(flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		if (sc->sc_flags & SCFLAG_OWRITE) {
			error = EBUSY;
			break;
		}
		sc->sc_flags |= SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		if (!(flag & FREAD) || (flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		sc->sc_flags |= SCFLAG_OREAD;
		break;
	default:
		error = ENXIO;
		break;
	}
	return error;
}

int
apmclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct pxa2x0_apm_softc *sc;

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
	return 0;
}

int
apmioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct pxa2x0_apm_softc *sc;
	struct pxaapm_battery_info batt;
	struct apm_power_info *power;
	int error = 0;

	/* apm0 only */
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
		return ENXIO;

	switch (cmd) {
		/* some ioctl names from linux */
	case APM_IOC_STANDBY:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			apm_userstandbys++;
		break;
	case APM_IOC_SUSPEND:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			apm_suspends++;	/* XXX */
		break;
	case APM_IOC_PRN_CTL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			int flag = *(int *)data;
			DPRINTF(( "APM_IOC_PRN_CTL: %d\n", flag ));
			switch (flag) {
			case APM_PRINT_ON:	/* enable printing */
				sc->sc_flags &= ~SCFLAG_PRINT;
				break;
			case APM_PRINT_OFF: /* disable printing */
				sc->sc_flags &= ~SCFLAG_PRINT;
				sc->sc_flags |= SCFLAG_NOPRINT;
				break;
			case APM_PRINT_PCT: /* disable some printing */
				sc->sc_flags &= ~SCFLAG_PRINT;
				sc->sc_flags |= SCFLAG_PCTPRINT;
				break;
			default:
				error = EINVAL;
				break;
			}
		}
		break;
	case APM_IOC_DEV_CTL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		break;
	case APM_IOC_GETPOWER:
	        power = (struct apm_power_info *)data;

		apm_battery_info(sc, &batt);

		power->ac_state = ((batt.flags & PXAAPM_AC_PRESENT) ?
		    APM_AC_ON : APM_AC_OFF);
		power->battery_life =
		    ((batt.cur_charge * 100) / batt.max_charge);

		/*
		 * If the battery is charging, return the minutes left until
		 * charging is complete. apmd knows this.
		 */

		if (!(batt.flags & PXAAPM_BATT_PRESENT)) {
			power->battery_state = APM_BATT_UNKNOWN;
			power->minutes_left = 0;
			power->battery_life = 0;
		} else if ((power->ac_state == APM_AC_ON) &&
			   (batt.draw > 0)) {
			power->minutes_left =
			    (((batt.max_charge - batt.cur_charge) * 3600) /
			    batt.draw) / 60;
			power->battery_state = APM_BATT_CHARGING;
		} else {
			power->minutes_left =
			    ((batt.cur_charge * 3600) / (-batt.draw)) / 60;

			/* XXX - Arbitrary */
			if (power->battery_life > 60)
				power->battery_state = APM_BATT_HIGH;
			else if (power->battery_life < 10)
				power->battery_state = APM_BATT_CRITICAL;
			else
				power->battery_state = APM_BATT_LOW;
		}
		break;

	default:
		error = ENOTTY;
	}

	return error;
}

void
filt_apmrdetach(struct knote *kn)
{
	struct pxa2x0_apm_softc *sc =
	    (struct pxa2x0_apm_softc *)kn->kn_hook;

	SLIST_REMOVE(&sc->sc_note, kn, knote, kn_selnext);
}

int
filt_apmread(struct knote *kn, long hint)
{
	/* XXX weird kqueue_scan() semantics */
	if (hint && !kn->kn_data)
		kn->kn_data = (int)hint;

	return (1);
}

int
apmkqfilter(dev_t dev, struct knote *kn)
{
	struct pxa2x0_apm_softc *sc;

	/* apm0 only */
	if (!apm_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = apm_cd.cd_devs[APMUNIT(dev)]))
		return ENXIO;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &apmread_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = (caddr_t)sc;
	SLIST_INSERT_HEAD(&sc->sc_note, kn, kn_selnext);

	return (0);
}

void
pxa2x0_apm_standby(struct pxa2x0_apm_softc *sc)
{
	int save;

	save = disable_interrupts(I32_bit|F32_bit);

	/* XXX add power hooks elsewhere, first. */

	restore_interrupts(save);
}

void
pxa2x0_apm_sleep(struct pxa2x0_apm_softc *sc)
{

	/* XXX first make standby work, and then sleep mode. */
	pxa2x0_apm_standby(sc);
}

void
pxa2x0_apm_attach_sub(struct pxa2x0_apm_softc *sc)
{

	sc->sc_iot = &pxa2x0_bs_tag;

	if (bus_space_map(sc->sc_iot, PXA2X0_POWMAN_BASE,
	    PXA2X0_POWMAN_SIZE, 0, &sc->sc_pm_ioh)) {
		printf("pxa2x0_apm_attach_sub: failed to map POWMAN\n");
		return;
	}

	lockinit(&sc->sc_lock, PWAIT, "apmlk", 0, 0);

	kthread_create_deferred(apm_thread_create, sc);

	printf("\n");
}
