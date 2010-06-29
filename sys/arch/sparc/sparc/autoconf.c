/*	$OpenBSD: autoconf.c,v 1.89 2010/06/29 21:28:10 miod Exp $	*/
/*	$NetBSD: autoconf.c,v 1.73 1997/07/29 09:41:53 fair Exp $ */

/*
 * Copyright (c) 1996
 *    The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <net/if.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#ifdef SUN4
#include <machine/oldmon.h>
#include <machine/idprom.h>
#include <sparc/sparc/memreg.h>
#endif
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/pmap.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/timerreg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif


/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	fbnode;		/* node ID of ROM's console frame buffer */
int	optionsnode;	/* node ID of ROM's options */
int	mmu_3l;		/* SUN4_400 models have a 3-level MMU */

#ifdef KGDB
extern	int kgdb_debug_panic;
#endif

static	int rootnode;
static	char *str2hex(char *, int *);
static	int mbprint(void *, const char *);
static	void crazymap(char *, int *);
void	sync_crash(void);
int	mainbus_match(struct device *, void *, void *);
static	void mainbus_attach(struct device *, struct device *, void *);

struct	bootpath bootpath[8];
int	nbootpath;
static	void bootpath_build(void);
static	void bootpath_fake(struct bootpath *, char *);
static	void bootpath_print(struct bootpath *);
int	search_prom(int, char *);
char	mainbus_model[30];

/* Translate SBus interrupt level to processor IPL */
int	intr_sbus2ipl_4c[] = {
	0, 1, 2, 3, 5, 7, 8, 9
};
int	intr_sbus2ipl_4m[] = {
	0, 2, 3, 5, 7, 9, 11, 13
};

/*
 * Convert hex ASCII string to a value.  Returns updated pointer.
 * Depends on ASCII order (this *is* machine-dependent code, you know).
 */
static char *
str2hex(str, vp)
	register char *str;
	register int *vp;
{
	register int v, c;

	for (v = 0;; v = v * 16 + c, str++) {
		c = *(u_char *)str;
		if (c <= '9') {
			if ((c -= '0') < 0)
				break;
		} else if (c <= 'F') {
			if ((c -= 'A' - 10) < 10)
				break;
		} else if (c <= 'f') {
			if ((c -= 'a' - 10) < 10)
				break;
		} else
			break;
	}
	*vp = v;
	return (str);
}

#ifdef SUN4
struct promvec promvecdat;
struct om_vector *oldpvec = (struct om_vector *)PROM_BASE;
#endif

/*
 * locore.s code calls bootstrap() just before calling main(), after double
 * mapping the kernel to high memory and setting up the trap base register.
 * We must finish mapping the kernel properly and glean any bootstrap info.
 */
void
bootstrap()
{
#if defined(SUN4)
	if (CPU_ISSUN4) {
		extern void oldmon_w_cmd(u_long, char *);

		/*
		 * XXX:
		 * The promvec is bogus. We need to build a
		 * fake one from scratch as soon as possible.
		 */
		bzero(&promvecdat, sizeof promvecdat);
		promvec = &promvecdat;

		promvec->pv_stdin = oldpvec->inSource;
		promvec->pv_stdout = oldpvec->outSink;
		promvec->pv_putchar = oldpvec->putChar;
		promvec->pv_putstr = oldpvec->fbWriteStr;
		promvec->pv_nbgetchar = oldpvec->mayGet;
		promvec->pv_getchar = oldpvec->getChar;
		promvec->pv_romvec_vers = 0;		/* eek! */
		promvec->pv_reboot = oldpvec->reBoot;
		promvec->pv_abort = oldpvec->abortEntry;
		promvec->pv_setctxt = oldpvec->setcxsegmap;
		promvec->pv_v0bootargs = (struct v0bootargs **)(oldpvec->bootParam);
		promvec->pv_halt = oldpvec->exitToMon;

		/*
		 * Discover parts of the machine memory organization
		 * that we need this early.
		 */
		if (oldpvec->romvecVersion >= 2)
			*oldpvec->vector_cmd = oldmon_w_cmd;
	}
#endif /* SUN4 */

	bzero(&cpuinfo, sizeof(struct cpu_softc));
	cpuinfo.master = 1;
	getcpuinfo(&cpuinfo, 0);

	pmap_bootstrap(cpuinfo.mmu_ncontext,
		       cpuinfo.mmu_nregion,
		       cpuinfo.mmu_nsegment);
	/* Moved zs_kgdb_init() to zs.c:consinit() */
#ifdef DDB
	db_machine_init();
	ddb_init();
#endif

	/*
	 * On sun4ms we have to do some nasty stuff here. We need to map
	 * in the interrupt registers (since we need to find out where
	 * they are from the PROM, since they aren't in a fixed place), and
	 * disable all interrupts. We can't do this easily from locore
	 * since the PROM is ugly to use from assembly. We also need to map
	 * in the counter registers because we can't disable the level 14
	 * (statclock) interrupt, so we need a handler early on (ugh).
	 *
	 * NOTE: We *demand* the psl to stay at splhigh() at least until
	 * we get here. The system _cannot_ take interrupts until we map
	 * the interrupt registers.
	 */

#if defined(SUN4M)
#define getpte4m(va)	lda(((va) & 0xFFFFF000) | ASI_SRMMUFP_L3, ASI_SRMMUFP)

	/* First we'll do the interrupt registers */
	if (CPU_ISSUN4M) {
		register int node;
		struct romaux ra;
		register u_int pte;
		register int i;
		extern void setpte4m(u_int, u_int);
		extern struct timer_4m *timerreg_4m;
		extern struct counter_4m *counterreg_4m;

		if ((node = opennode("/obio/interrupt")) == 0)
		    if ((node=search_prom(findroot(),"interrupt"))==0)
			panic("bootstrap: could not get interrupt "
			      "node from prom");

		if (!romprop(&ra, "interrupt", node))
		    panic("bootstrap: could not get interrupt properties");
		if (ra.ra_nvaddrs < 2)
		    panic("bootstrap: less than 2 interrupt regs. available");
		if (ra.ra_nvaddrs > 5)
		    panic("bootstrap: cannot support capability of > 4 CPUs");

		for (i = 0; i < ra.ra_nvaddrs - 1; i++) {

			pte = getpte4m((u_int)ra.ra_vaddrs[i]);
			if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
			    panic("bootstrap: PROM has invalid mapping for "
				  "processor interrupt register %d",i);
			pte |= PPROT_S;

			/* Duplicate existing mapping */

			setpte4m(PI_INTR_VA + (_MAXNBPG * i), pte);
		}

		/*
		 * That was the processor register...now get system register;
		 * it is the last returned by the PROM
		 */
		pte = getpte4m((u_int)ra.ra_vaddrs[i]);
		if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
		    panic("bootstrap: PROM has invalid mapping for system "
			  "interrupt register");
		pte |= PPROT_S;

		setpte4m(SI_INTR_VA, pte);

		/* Now disable interrupts */
		ienab_bis(SINTR_MA);

		/* Send all interrupts to primary processor */
		*((u_int *)ICR_ITR) = 0;

#ifdef DEBUG
/*		printf("SINTR: mask: 0x%x, pend: 0x%x\n", *(int *)ICR_SI_MASK,
		       *(int *)ICR_SI_PEND);
*/
#endif

		/*
		 * Now map in the counters
		 * (XXX: fix for multiple CPUs! We assume 1)
		 * The processor register is the first; the system is the last.
		 * See also timerattach() in clock.c.
		 * This shouldn't be necessary; we ought to keep interrupts off
		 * and/or disable the (level 14) counter...
		 */

		if ((node = opennode("/obio/counter")) == 0)
		    if ((node=search_prom(findroot(),"counter"))==0)
			panic("bootstrap: could not find counter in OPENPROM");

		if (!romprop(&ra, "counter", node))
			panic("bootstrap: could not find counter properties");

		counterreg_4m = (struct counter_4m *)ra.ra_vaddrs[0];
		timerreg_4m = (struct timer_4m *)ra.ra_vaddrs[ra.ra_nvaddrs-1];
	}
#endif /* SUN4M */

	if (CPU_ISSUN4OR4C) {
		/* Map Interrupt Enable Register */
		pmap_kenter_pa(INTRREG_VA,
			   INT_ENABLE_REG_PHYSADR | PMAP_NC | PMAP_OBIO,
			   VM_PROT_READ | VM_PROT_WRITE);
		pmap_update(pmap_kernel());
		/* Disable all interrupts */
		*((unsigned char *)INTRREG_VA) = 0;
	}
}

/*
 * bootpath_build: build a bootpath. Used when booting a generic
 * kernel to find our root device.  Newer proms give us a bootpath,
 * for older proms we have to create one.  An element in a bootpath
 * has 4 fields: name (device name), val[0], val[1], and val[2]. Note that:
 * Interpretation of val[] is device-dependent. Some examples:
 *
 * if (val[0] == -1) {
 *	val[1] is a unit number    (happens most often with old proms)
 * } else {
 *	[sbus device] val[0] is a sbus slot, and val[1] is an sbus offset
 *	[scsi disk] val[0] is target, val[1] is lun, val[2] is partition
 *	[scsi tape] val[0] is target, val[1] is lun, val[2] is file #
 * }
 *
 */

static void
bootpath_build()
{
	register char *cp, *pp;
	register struct bootpath *bp;

	/*
	 * On SS1s, promvec->pv_v0bootargs->ba_argv[1] contains the flags
	 * that were given after the boot command.  On SS2s, pv_v0bootargs
	 * is NULL but *promvec->pv_v2bootargs.v2_bootargs points to
	 * "vmunix -s" or whatever.
	 * XXX	DO THIS BEFORE pmap_bootstrap?
	 */
	bzero(bootpath, sizeof(bootpath));
	bp = bootpath;
	if (promvec->pv_romvec_vers < 2) {
		/*
		 * Grab boot device name and values.  build fake bootpath.
		 */
		cp = (*promvec->pv_v0bootargs)->ba_argv[0];

		if (cp != NULL)
			bootpath_fake(bp, cp);

		/* Setup pointer to boot flags */
		cp = (*promvec->pv_v0bootargs)->ba_argv[1];
		if (cp == NULL || *cp != '-')
			return;
	} else {
		/*
		 * Grab boot path from PROM
		 */
		cp = *promvec->pv_v2bootargs.v2_bootpath;
		while (cp != NULL && *cp == '/') {
			/* Step over '/' */
			++cp;
			/* Extract name */
			pp = bp->name;
			while (*cp != '@' && *cp != '/' && *cp != '\0')
				*pp++ = *cp++;
			*pp = '\0';
			if (*cp == '@') {
				cp = str2hex(++cp, &bp->val[0]);
				if (*cp == ',')
					cp = str2hex(++cp, &bp->val[1]);
				if (*cp == ':') {
					/*
					 * We only store one character here,
					 * as we will only use this field
					 * to compute a partition index
					 * for block devices.  However, it
					 * might be an ethernet media
					 * specification, so be sure to
					 * skip all letters.
					 */
					bp->val[2] = *++cp - 'a';
					while (*cp != '\0' && *cp != '/')
						cp++;
				}
			} else {
				bp->val[0] = -1; /* no #'s: assume unit 0, no
							sbus offset/address */
			}
			++bp;
			++nbootpath;
		}
		bp->name[0] = 0;

		/* Setup pointer to boot flags */
		cp = *promvec->pv_v2bootargs.v2_bootargs;
		if (cp == NULL)
			return;
		while (*cp != '-')
			if (*cp++ == '\0')
				return;
	}
	for (;;) {
		switch (*++cp) {

		case '\0':
			return;

		case 'a':
			boothowto |= RB_ASKNAME;
			break;

		case 'c':
			boothowto |= RB_CONFIG;
			break;

		case 'd':	/* kgdb - always on zs	XXX */
#ifdef KGDB
			boothowto |= RB_KDB;	/* XXX unused */
			kgdb_debug_panic = 1;
			kgdb_connect(1);
#elif DDB
			Debugger();
#else
			printf("kernel has no debugger\n");
#endif
			break;

		case 's':
			boothowto |= RB_SINGLE;
			break;
		}
	}
}

/*
 * Fake a ROM generated bootpath.
 * The argument `cp' points to a string such as "xd(0,0,0)bsd"
 */

static void
bootpath_fake(bp, cp)
	struct bootpath *bp;
	char *cp;
{
	register char *pp;
	int v0val[3];

#define BP_APPEND(BP,N,V0,V1,V2) { \
	strlcpy((BP)->name, N, sizeof (BP)->name); \
	(BP)->val[0] = (V0); \
	(BP)->val[1] = (V1); \
	(BP)->val[2] = (V2); \
	(BP)++; \
	nbootpath++; \
}

#if defined(SUN4)
	if (CPU_ISSUN4M) {
		printf("twas brillig..\n");
		return;
	}
#endif

	pp = cp + 2;
	v0val[0] = v0val[1] = v0val[2] = 0;
	if (*pp == '(' 					/* for vi: ) */
 	    && *(pp = str2hex(++pp, &v0val[0])) == ','
	    && *(pp = str2hex(++pp, &v0val[1])) == ',')
		(void)str2hex(++pp, &v0val[2]);

#if defined(SUN4)
	if (CPU_ISSUN4) {
		char tmpname[8];

		/*
		 *  xylogics VME dev: xd, xy, xt
		 *  fake looks like: /vmel0/xdc0/xd@1,0
		 */
		if (cp[0] == 'x') {
			if (cp[1] == 'd') {/* xd? */
				BP_APPEND(bp, "vmel", -1, 0, 0);
			} else {
				BP_APPEND(bp, "vmes", -1, 0, 0);
			}
			snprintf(tmpname,sizeof tmpname,"x%cc", cp[1]); /* e.g. xdc */
			BP_APPEND(bp, tmpname,-1, v0val[0], 0);
			snprintf(tmpname,sizeof tmpname,"%c%c", cp[0], cp[1]);
			BP_APPEND(bp, tmpname,v0val[1], v0val[2], 0); /* e.g. xd */
			return;
		}

		/*
		 * ethernet: ie, le (rom supports only obio?)
		 * fake looks like: /obio0/le0
		 */
		if ((cp[0] == 'i' || cp[0] == 'l') && cp[1] == 'e')  {
			BP_APPEND(bp, "obio", -1, 0, 0);
			snprintf(tmpname,sizeof tmpname,"%c%c", cp[0], cp[1]);
			BP_APPEND(bp, tmpname, -1, 0, 0);
			return;
		}

		/*
		 * scsi: sd, st, sr
		 * assume: 4/100 = sw: /obio0/sw0/sd@0,0:a
		 * 4/200 & 4/400 = si/sc: /vmes0/si0/sd@0,0:a
 		 * 4/300 = esp: /obio0/esp0/sd@0,0:a
		 * (note we expect sc to mimic an si...)
		 */
		if (cp[0] == 's' &&
			(cp[1] == 'd' || cp[1] == 't' || cp[1] == 'r')) {

			int  target, lun;

			switch (cpuinfo.cpu_type) {
			case CPUTYP_4_200:
			case CPUTYP_4_400:
				BP_APPEND(bp, "vmes", -1, 0, 0);
				BP_APPEND(bp, "si", -1, v0val[0], 0);
				break;
			case CPUTYP_4_100:
				BP_APPEND(bp, "obio", -1, 0, 0);
				BP_APPEND(bp, "sw", -1, v0val[0], 0);
				break;
			case CPUTYP_4_300:
				BP_APPEND(bp, "obio", -1, 0, 0);
				BP_APPEND(bp, "esp", -1, v0val[0], 0);
				break;
			default:
				panic("bootpath_fake: unknown system type %d",
				      cpuinfo.cpu_type);
			}
			/*
			 * Deal with target/lun encodings.
			 * Note: more special casing in device_register().
			 */
			if (oldpvec->monId[0] > '1') {
				target = v0val[1] >> 3; /* new format */
				lun    = v0val[1] & 0x7;
			} else {
				target = v0val[1] >> 2; /* old format */
				lun    = v0val[1] & 0x3;
			}
			snprintf(tmpname, sizeof tmpname, "%c%c", cp[0], cp[1]);
			BP_APPEND(bp, tmpname, target, lun, v0val[2]);
			return;
		}

		return; /* didn't grok bootpath, no change */
	}
#endif /* SUN4 */

#if defined(SUN4C)
	/*
	 * sun4c stuff
	 */

	/*
	 * floppy: fd
	 * fake looks like: /fd@0,0:a
	 */
	if (cp[0] == 'f' && cp[1] == 'd') {
		/*
		 * Assume `fd(c,u,p)' means:
		 * partition `p' on floppy drive `u' on controller `c'
		 */
		BP_APPEND(bp, "fd", v0val[0], v0val[1], v0val[2]);
		return;
	}

	/*
	 * ethernet: le
	 * fake looks like: /sbus0/le0
	 */
	if (cp[0] == 'l' && cp[1] == 'e') {
		BP_APPEND(bp, "sbus", -1, 0, 0);
		BP_APPEND(bp, "le", -1, v0val[0], 0);
		return;
	}

	/*
	 * scsi: sd, st, sr
	 * fake looks like: /sbus0/esp0/sd@3,0:a
	 */
	if (cp[0] == 's' && (cp[1] == 'd' || cp[1] == 't' || cp[1] == 'r')) {
		char tmpname[8];
		int  target, lun;

		BP_APPEND(bp, "sbus", -1, 0, 0);
		BP_APPEND(bp, "esp", -1, v0val[0], 0);
		if (cp[1] == 'r')
			snprintf(tmpname, sizeof tmpname, "cd"); /* OpenBSD uses 'cd', not 'sr'*/
		else
			snprintf(tmpname, sizeof tmpname, "%c%c", cp[0], cp[1]);
		/* XXX - is TARGET/LUN encoded in v0val[1]? */
		target = v0val[1];
		lun = 0;
		BP_APPEND(bp, tmpname, target, lun, v0val[2]);
		return;
	}
#endif /* SUN4C */


	/*
	 * unknown; return
	 */

#undef BP_APPEND
}

/*
 * print out the bootpath
 * the %x isn't 0x%x because the Sun EPROMs do it this way, and
 * consistency with the EPROMs is probably better here.
 */

static void
bootpath_print(bp)
	struct bootpath *bp;
{
	printf("bootpath: ");
	while (bp->name[0]) {
		if (bp->val[0] == -1)
			printf("/%s%x", bp->name, bp->val[1]);
		else
			printf("/%s@%x,%x", bp->name, bp->val[0], bp->val[1]);
		if (bp->val[2] != 0)
			printf(":%c", bp->val[2] + 'a');
		bp++;
	}
	printf("\n");
}


/*
 * save or read a bootpath pointer from the boothpath store.
 *
 * XXX. required because of SCSI... we don't have control over the "sd"
 * device, so we can't set boot device there.   we patch in with
 * device_register(), and use this to recover the bootpath.
 */

struct bootpath *
bootpath_store(storep, bp)
	int storep;
	struct bootpath *bp;
{
	static struct bootpath *save;
	struct bootpath *retval;

	retval = save;
	if (storep)
		save = bp;

	return (retval);
}

/*
 * Set up the sd target mappings for non SUN4 PROMs.
 * Find out about the real SCSI target, given the PROM's idea of the
 * target of the (boot) device (i.e., the value in bp->v0val[0]).
 */
static void
crazymap(prop, map)
	char *prop;
	int *map;
{
	int i;
	char *propval;

	if (!CPU_ISSUN4 && promvec->pv_romvec_vers < 2) {
		/*
		 * Machines with real v0 proms have an `s[dt]-targets' property
		 * which contains the mapping for us to use. v2 proms donot
		 * require remapping.
		 */
		propval = getpropstring(optionsnode, prop);
		if (propval == NULL || strlen(propval) != 8) {
 build_default_map:
			printf("WARNING: %s map is bogus, using default\n",
				prop);
			for (i = 0; i < 8; ++i)
				map[i] = i;
			i = map[0];
			map[0] = map[3];
			map[3] = i;
			return;
		}
		for (i = 0; i < 8; ++i) {
			map[i] = propval[i] - '0';
			if (map[i] < 0 ||
			    map[i] >= 8)
				goto build_default_map;
		}
	} else {
		/*
		 * Set up the identity mapping for old sun4 monitors
		 * and v[2-] OpenPROMs. Note: device_register() does the
		 * SCSI-target juggling for sun4 monitors.
		 */
		for (i = 0; i < 8; ++i)
			map[i] = i;
	}
}

int
sd_crazymap(n)
	int	n;
{
	static int prom_sd_crazymap[8]; /* static: compute only once! */
	static int init = 0;

	if (init == 0) {
		crazymap("sd-targets", prom_sd_crazymap);
		init = 1;
	}
	return prom_sd_crazymap[n];
}

/*
 * Determine mass storage and memory configuration for a machine.
 * We get the PROM's root device and make sure we understand it, then
 * attach it as `mainbus0'.  We also set up to handle the PROM `sync'
 * command.
 */
void
cpu_configure()
{
	struct confargs oca;
	register int node = 0;
	register char *cp;
	int s;
	extern struct user *proc0paddr;

	/* build the bootpath */
	bootpath_build();

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}

#if defined(SUN4)
	if (CPU_ISSUN4) {
		extern struct cfdata cfdata[];
		extern struct cfdriver memreg_cd, obio_cd;
		struct cfdata *cf, *memregcf = NULL;
		register short *p;
		struct rom_reg rr;

		for (cf = cfdata; memregcf==NULL && cf->cf_driver; cf++) {
			if (cf->cf_driver != &memreg_cd ||
				cf->cf_loc[0] == -1) /* avoid sun4m memreg0 */
				continue;
			for (p = cf->cf_parents; memregcf==NULL && *p >= 0; p++)
				if (cfdata[*p].cf_driver == &obio_cd)
					memregcf = cf;
		}
		if (memregcf == NULL)
			panic("cpu_configure: no memreg found!");

		rr.rr_iospace = PMAP_OBIO;
		rr.rr_paddr = (void *)memregcf->cf_loc[0];
		rr.rr_len = NBPG;
		par_err_reg = (u_int *)bus_map(&rr, NBPG);
		if (par_err_reg == NULL)
			panic("cpu_configure: ROM hasn't mapped memreg!");
	}
#endif
#if defined(SUN4C)
	if (CPU_ISSUN4C) {
		node = findroot();
		cp = getpropstring(node, "device_type");
		if (strcmp(cp, "cpu") != 0)
			panic("PROM root device type = %s (need CPU)", cp);
	}
#endif
#if defined(SUN4M)
	if (CPU_ISSUN4M)
		node = findroot();
#endif

	if (!CPU_ISSUN4)
		*promvec->pv_synchook = sync_crash;

	oca.ca_ra.ra_node = node;
	oca.ca_ra.ra_name = cp = "mainbus";
	if (config_rootfound(cp, (void *)&oca) == NULL)
		panic("mainbus not configured");

	/* Enable device interrupts */
#if defined(SUN4M)
	if (CPU_ISSUN4M)
		ienab_bic(SINTR_MA);
#endif
#if defined(SUN4) || defined(SUN4C)
	if (CPU_ISSUN4OR4C)
		ienab_bis(IE_ALLIE);
#endif
	(void)spl0();

	/*
	 * Re-zero proc0's user area, to nullify the effect of the
	 * stack running into it during auto-configuration.
	 * XXX - should fix stack usage.
	 */
	s = splhigh();
	bzero(proc0paddr, sizeof(struct user));

	pmap_redzone();
	splx(s);
	cold = 0;
}

void
diskconf(void)
{
	struct bootpath *bp;
	struct device *bootdv;

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	bootpath_print(bootpath);

	bp = nbootpath == 0 ? NULL : &bootpath[nbootpath-1];
	bootdv = (bp == NULL) ? NULL : bp->dev;

	setroot(bootdv, bp->val[2], RB_USERREQ | RB_HALT);
	dumpconf();
}

/*
 * Console `sync' command.  SunOS just does a `panic: zero' so I guess
 * no one really wants anything fancy...
 */
void
sync_crash()
{

	panic("PROM sync command");
}

char *
clockfreq(freq)
	register int freq;
{
	register char *p;
	static char buf[10];

	freq /= 1000;
	snprintf(buf, sizeof buf, "%d", freq / 1000);
	freq %= 1000;
	if (freq) {
		freq += 1000;	/* now in 1000..1999 */
		p = buf + strlen(buf);
		snprintf(p, buf + sizeof buf - p, "%d", freq);
		*p = '.';	/* now buf = %d.%3d */
	}
	return (buf);
}

/* ARGSUSED */
static int
mbprint(aux, name)
	void *aux;
	const char *name;
{
	register struct confargs *ca = aux;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	if (ca->ca_ra.ra_paddr)
		printf(" %saddr 0x%x", ca->ca_ra.ra_iospace ? "io" : "",
		    (int)ca->ca_ra.ra_paddr);
	return (UNCONF);
}

int
findroot()
{
	register int node;

	if ((node = rootnode) == 0 && (node = nextsibling(0)) == 0)
		panic("no PROM root device");
	rootnode = node;
	return (node);
}

/*
 * Given a `first child' node number, locate the node with the given name.
 * Return the node number, or 0 if not found.
 */
int
findnode(first, name)
	int first;
	register const char *name;
{
	register int node;

	for (node = first; node; node = nextsibling(node))
		if (strcmp(getpropstring(node, "name"), name) == 0)
			return (node);
	return (0);
}

/*
 * Fill in a romaux.  Returns 1 on success, 0 if the register property
 * was not the right size.
 */
int
romprop(rp, cp, node)
	register struct romaux *rp;
	const char *cp;
	register int node;
{
	int len, n, intr;
	union { char regbuf[256]; struct rom_reg rr[RA_MAXREG]; } u;
	static const char pl[] = "property length";

	bzero(u.regbuf, sizeof u);
	len = getprop(node, "reg", (void *)u.regbuf, sizeof(u.regbuf));
	if (len == -1 &&
	    node_has_property(node, "device_type") &&
	    strcmp(getpropstring(node, "device_type"), "hierarchical") == 0)
		len = 0;
	if (len % sizeof(struct rom_reg)) {
		printf("%s \"reg\" %s = %d (need multiple of %d)\n",
			cp, pl, len, sizeof(struct rom_reg));
		return (0);
	}
	if (len > RA_MAXREG * sizeof(struct rom_reg))
		printf("warning: %s \"reg\" %s %d > %d, excess ignored\n",
		    cp, pl, len, RA_MAXREG * sizeof(struct rom_reg));
	rp->ra_node = node;
	rp->ra_name = cp;
	rp->ra_nreg = len / sizeof(struct rom_reg);
	bcopy(u.rr, rp->ra_reg, len);

	len = getprop(node, "address", (void *)rp->ra_vaddrs,
		      sizeof(rp->ra_vaddrs));
	if (len == -1) {
		rp->ra_vaddr = 0;	/* XXX - driver compat */
		len = 0;
	}
	if (len & 3) {
		printf("%s \"address\" %s = %d (need multiple of 4)\n",
		    cp, pl, len);
		len = 0;
	}
	rp->ra_nvaddrs = len >> 2;

	len = getprop(node, "intr", (void *)&rp->ra_intr, sizeof rp->ra_intr);
	if (len == -1)
		len = 0;

	/*
	 * Some SBus cards only provide an "interrupts" properly, listing
	 * SBus levels. But since obio devices will usually also provide
	 * both properties, only check for "interrupts" last.
	 */
	if (len == 0) {
		u_int32_t *interrupts;
		len = getproplen(node, "interrupts");
		if (len > 0 &&
		    (interrupts = malloc(len, M_TEMP, M_NOWAIT)) != NULL) {
			/* Build rom_intr structures from the list */
			getprop(node, "interrupts", interrupts, len);
			len /= sizeof(u_int32_t);
			for (n = 0; n < len; n++) {
				intr = interrupts[n];
				/*
				 * Non-SBus devices (such as the cgfourteen,
				 * which attaches on obio) do not need their
				 * interrupt level translated.
				 */
				if (intr < 8) {
					intr = CPU_ISSUN4M ?
					    intr_sbus2ipl_4m[intr] :
					    intr_sbus2ipl_4c[intr];
				}
				rp->ra_intr[n].int_pri = intr;
				rp->ra_intr[n].int_vec = 0;
			};
			len *= sizeof(struct rom_intr);
			free(interrupts, M_TEMP);
		} else
			len = 0;
	}

	if (len & 7) {
		printf("%s \"intr\" %s = %d (need multiple of 8)\n",
		    cp, pl, len);
		len = 0;
	}
	rp->ra_nintr = len >>= 3;
	/* SPARCstation interrupts are not hardware-vectored */
	while (--len >= 0) {
		if (rp->ra_intr[len].int_vec) {
			printf("WARNING: %s interrupt %d has nonzero vector\n",
			    cp, len);
			break;
		}
#if defined(SUN4M)
		if (CPU_ISSUN4M) {
			/* What's in these high bits anyway? */
			rp->ra_intr[len].int_pri &= 0xf;
		}
#endif

	}
	return (1);
}

int
mainbus_match(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	struct cfdata *cf = self;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

int autoconf_nzs = 0;	/* must be global so obio.c can see it */

/*
 * Attach the mainbus.
 *
 * Our main job is to attach the CPU (the root node we got in cpu_configure())
 * and iterate down the list of `mainbus devices' (children of that node).
 * We also record the `node id' of the default frame buffer, if any.
 */
static void
mainbus_attach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	struct confargs oca;
	register const char *const *ssp, *sp = NULL;
	struct confargs *ca = aux;
#if defined(SUN4C) || defined(SUN4M)
	register int node0, node;
	const char *const *openboot_special;
#define L1A_HACK		/* XXX hack to allow L1-A during autoconf */
#ifdef L1A_HACK
	int audio = 0;
#endif
#endif
#if defined(SUN4)
	static const char *const oldmon_special[] = {
		"vmel",
		"vmes",
		"led",
		NULL
	};
#endif /* SUN4 */

#if defined(SUN4C)
	static const char *const openboot_special4c[] = {
		/* find these first (end with empty string) */
		"memory-error",	/* as early as convenient, in case of error */
		"eeprom",
		"counter-timer",
		"auxiliary-io",
		"",

		/* ignore these (end with NULL) */
		"aliases",
		"interrupt-enable",
		"memory",
		"openprom",
		"options",
		"packages",
		"virtual-memory",
		NULL
	};
#else
#define openboot_special4c	((void *)0)
#endif
#if defined(SUN4M)
	static const char *const openboot_special4m[] = {
		/* find these first */
		"obio",		/* smart enough to get eeprom/etc mapped */
		"",

		/* ignore these (end with NULL) */
		/*
		 * These are _root_ devices to ignore. Others must be handled
		 * elsewhere.
		 */
		"SUNW,sx",		/* XXX: no driver for SX yet */
		"eccmemctl",
		"virtual-memory",
		"aliases",
		"memory",
		"openprom",
		"options",
		"packages",
		/* we also skip any nodes with device_type == "cpu" */
		NULL
	};
#else
#define openboot_special4m	((void *)0)
#endif

	if (CPU_ISSUN4)
		snprintf(mainbus_model, sizeof mainbus_model,
			"SUN-4/%d series", cpuinfo.classlvl);
	else
		strlcat(mainbus_model, getpropstring(ca->ca_ra.ra_node,"name"),
			sizeof mainbus_model);
	printf(": %s\n", mainbus_model);

	/*
	 * Locate and configure the ``early'' devices.  These must be
	 * configured before we can do the rest.  For instance, the
	 * EEPROM contains the Ethernet address for the LANCE chip.
	 * If the device cannot be located or configured, panic.
	 */

#if defined(SUN4)
	if (CPU_ISSUN4) {
		/* Configure the CPU. */
		bzero(&oca, sizeof(oca));
		oca.ca_ra.ra_name = "cpu";
		(void)config_found(dev, (void *)&oca, mbprint);

		/* Start at the beginning of the bootpath */
		bzero(&oca, sizeof(oca));
		oca.ca_ra.ra_bp = bootpath;

		oca.ca_bustype = BUS_MAIN;
		oca.ca_ra.ra_name = "obio";
		if (config_found(dev, (void *)&oca, mbprint) == NULL)
			panic("obio missing");

		for (ssp = oldmon_special; (sp = *ssp) != NULL; ssp++) {
			oca.ca_bustype = BUS_MAIN;
			oca.ca_ra.ra_name = sp;
			(void)config_found(dev, (void *)&oca, mbprint);
		}
		return;
	}
#endif

/*
 * The rest of this routine is for OBP machines exclusively.
 */
#if defined(SUN4C) || defined(SUN4M)

	openboot_special = CPU_ISSUN4M
				? openboot_special4m
				: openboot_special4c;

	node = ca->ca_ra.ra_node;	/* i.e., the root node */

	/* the first early device to be configured is the cpu */
	if (CPU_ISSUN4M) {
		/* XXX - what to do on multiprocessor machines? */
		register const char *cp;

		for (node = firstchild(node); node; node = nextsibling(node)) {
			cp = getpropstring(node, "device_type");
			if (strcmp(cp, "cpu") == 0) {
				bzero(&oca, sizeof(oca));
				oca.ca_ra.ra_node = node;
				oca.ca_ra.ra_name = "cpu";
				oca.ca_ra.ra_paddr = 0;
				oca.ca_ra.ra_nreg = 0;
				config_found(dev, (void *)&oca, mbprint);
			}
		}
	} else if (CPU_ISSUN4C) {
		bzero(&oca, sizeof(oca));
		oca.ca_ra.ra_node = node;
		oca.ca_ra.ra_name = "cpu";
		oca.ca_ra.ra_paddr = 0;
		oca.ca_ra.ra_nreg = 0;
		config_found(dev, (void *)&oca, mbprint);
	}

	node = ca->ca_ra.ra_node;	/* re-init root node */

	if (promvec->pv_romvec_vers <= 2) {
		/*
		 * Revision 1 prom will always return a framebuffer device
		 * node if a framebuffer is installed, even if console is
		 * set to serial.
		 */
		if (*promvec->pv_stdout != PROMDEV_SCREEN ||
		    *promvec->pv_stdin != PROMDEV_KBD)
			fbnode = 0;
		else {
			/* remember which frame buffer is the console */
			fbnode = getpropint(node, "fb", 0);
		}
	} else {
		/* fbnode already initialized in consinit() */
	}

	/* Find the "options" node */
	node0 = firstchild(node);
	optionsnode = findnode(node0, "options");
	if (optionsnode == 0)
		panic("no options in OPENPROM");

	/* Start at the beginning of the bootpath */
	oca.ca_ra.ra_bp = bootpath;

	for (ssp = openboot_special; *(sp = *ssp) != 0; ssp++) {
		if ((node = findnode(node0, sp)) == 0) {
			printf("could not find %s in OPENPROM\n", sp);
			panic(sp);
		}
		oca.ca_bustype = BUS_MAIN;
		if (!romprop(&oca.ca_ra, sp, node) ||
		    (config_found(dev, (void *)&oca, mbprint) == NULL))
			panic(sp);
	}

	/*
	 * Configure the rest of the devices, in PROM order.  Skip
	 * PROM entries that are not for devices, or which must be
	 * done before we get here.
	 */
	for (node = node0; node; node = nextsibling(node)) {
		register const char *cp;

#if defined(SUN4M)
		if (CPU_ISSUN4M) /* skip the CPUs */
			if (node_has_property(node, "device_type") &&
			    !strcmp(getpropstring(node, "device_type"), "cpu"))
				continue;
#endif
		cp = getpropstring(node, "name");
		for (ssp = openboot_special; (sp = *ssp) != NULL; ssp++)
			if (strcmp(cp, sp) == 0)
				break;
		if (sp == NULL && romprop(&oca.ca_ra, cp, node)) {
#ifdef L1A_HACK
			if (strcmp(cp, "audio") == 0)
				audio = 1;
			if (strcmp(cp, "zs") == 0)
				autoconf_nzs++;
			if (/*audio &&*/ autoconf_nzs >= 2)	/*XXX*/
				splx(11 << 8);		/*XXX*/
#endif
			oca.ca_bustype = BUS_MAIN;
			(void) config_found(dev, (void *)&oca, mbprint);
		}
	}
#endif /* SUN4C || SUN4M */
}

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

/*
 * findzs() is called from the zs driver (which is, at least in theory,
 * generic to any machine with a Zilog ZSCC chip).  It should return the
 * address of the corresponding zs channel.  It may not fail, and it
 * may be called before the VM code can be used.  Here we count on the
 * FORTH PROM to map in the required zs chips.
 */
void *
findzs(zs)
	int zs;
{

#if defined(SUN4)
#define ZS0_PHYS	0xf1000000
#define ZS1_PHYS	0xf0000000
#define ZS2_PHYS	0xe0000000

	if (CPU_ISSUN4) {
		struct rom_reg rr;
		register void *vaddr;

		switch (zs) {
		case 0:
			rr.rr_paddr = (void *)ZS0_PHYS;
			break;
		case 1:
			rr.rr_paddr = (void *)ZS1_PHYS;
			break;
		case 2:
			rr.rr_paddr = (void *)ZS2_PHYS;
			break;
		default:
			panic("findzs: unknown zs device %d", zs);
		}

		rr.rr_iospace = PMAP_OBIO;
		rr.rr_len = NBPG;
		vaddr = bus_map(&rr, NBPG);
		if (vaddr)
			return (vaddr);
	}
#endif

#if defined(SUN4C) || defined(SUN4M)
	if (CPU_ISSUN4COR4M) {
		int node;

		node = firstchild(findroot());
		if (CPU_ISSUN4M) { /* zs is in "obio" tree on Sun4M */
			node = findnode(node, "obio");
			if (!node)
				panic("findzs: no obio node");
			node = firstchild(node);
		}
		while ((node = findnode(node, "zs")) != 0) {
			int vaddrs[10];

			if (getpropint(node, "slave", -1) != zs) {
				node = nextsibling(node);
				continue;
			}

			/*
			 * On some machines (e.g. the Voyager), the zs
			 * device has multi-valued register properties.
			 */
			if (getprop(node, "address",
			    (void *)vaddrs, sizeof(vaddrs)) != 0)
				return ((void *)vaddrs[0]);
		}
		return (NULL);
	}
#endif
	panic("findzs: cannot find zs%d", zs);
	/* NOTREACHED */
}

int
makememarr(struct memarr *ap, u_int xmax, int which)
{
#if defined(SUN4C) || defined(SUN4M)
	struct v0mlist *mp;
	int node, n;
	char *prop;
#endif

#ifdef DIAGNOSTIC
	if (which != MEMARR_AVAILPHYS && which != MEMARR_TOTALPHYS)
		panic("makememarr");
#endif

#if defined(SUN4)
	if (CPU_ISSUN4) {
		if (ap != NULL && xmax != 0) {
			ap[0].addr_hi = 0;
			ap[0].addr_lo = 0;
			ap[0].len = which == MEMARR_AVAILPHYS ?
			    *oldpvec->memoryAvail : *oldpvec->memorySize;
		}
		return 1;
	}
#endif
#if defined(SUN4C) || defined(SUN4M)
	switch (n = promvec->pv_romvec_vers) {
	case 0:
		/*
		 * Version 0 PROMs use a linked list to describe these
		 * guys.
		 */
		mp = which == MEMARR_AVAILPHYS ?
		    *promvec->pv_v0mem.v0_physavail :
		    *promvec->pv_v0mem.v0_phystot;

		for (n = 0; mp != NULL; mp = mp->next, n++) {
			if (ap == NULL || n >= xmax)
				continue;
			ap->addr_hi = 0;
			ap->addr_lo = (uint32_t)mp->addr;
			ap->len = mp->nbytes;
			ap++;
		}
		break;
	default:
		printf("makememarr: hope version %d PROM is like version 2\n",
		    n);
		/* FALLTHROUGH */
	case 3:
	case 2:
		/*
		 * Version 2 PROMs use a property array to describe them.
		 */
		if ((node = findnode(firstchild(findroot()), "memory")) == 0)
			panic("makememarr: cannot find \"memory\" node");
		prop = which == MEMARR_AVAILPHYS ? "available" : "reg";
		n = getproplen(node, prop) / sizeof(struct memarr);
		if (ap != NULL) {
			if (getprop(node, prop, ap,
			    xmax * sizeof(struct memarr)) <= 0)
				panic("makememarr: cannot get property");
		}
		break;
	}

	if (n <= 0)
		panic("makememarr: no memory found");
	return (n);
#endif	/* SUN4C || SUN4M */
}

/*
 * Internal form of getprop().  Returns the actual length.
 */
int
getprop(node, name, buf, bufsiz)
	int node;
	char *name;
	void *buf;
	register int bufsiz;
{
#if defined(SUN4C) || defined(SUN4M)
	register struct nodeops *no;
	register int len;
#endif

#if defined(SUN4)
	if (CPU_ISSUN4) {
		printf("WARNING: getprop not valid on sun4! %s\n", name);
		return (0);
	}
#endif

#if defined(SUN4C) || defined(SUN4M)
	no = promvec->pv_nodeops;
	len = no->no_proplen(node, name);
	if (len > bufsiz) {
		printf("node 0x%x property %s length %d > %d\n",
		    node, name, len, bufsiz);
#ifdef DEBUG
		panic("getprop");
#else
		return (0);
#endif
	}
	no->no_getprop(node, name, buf);
	return (len);
#endif
}

/*
 * Internal form of proplen().  Returns the property length.
 */
int
getproplen(node, name)
	int node;
	char *name;
{
	register struct nodeops *no = promvec->pv_nodeops;

	return (no->no_proplen(node, name));
}

/*
 * Return a string property.  There is a (small) limit on the length;
 * the string is fetched into a static buffer which is overwritten on
 * subsequent calls.
 */
char *
getpropstring(node, name)
	int node;
	char *name;
{
	register int len;
	static char stringbuf[32];

	len = getprop(node, name, (void *)stringbuf, sizeof stringbuf - 1);
	if (len == -1)
		len = 0;
	stringbuf[len] = '\0';	/* usually unnecessary */
	return (stringbuf);
}

/*
 * Fetch an integer (or pointer) property.
 * The return value is the property, or the default if there was none.
 */
int
getpropint(node, name, deflt)
	int node;
	char *name;
	int deflt;
{
	register int len;
	char intbuf[16];

	len = getprop(node, name, (void *)intbuf, sizeof intbuf);
	if (len != 4)
		return (deflt);
	return (*(int *)intbuf);
}

/*
 * OPENPROM functions.  These are here mainly to hide the OPENPROM interface
 * from the rest of the kernel.
 */
int
firstchild(node)
	int node;
{

	return (promvec->pv_nodeops->no_child(node));
}

int
nextsibling(node)
	int node;
{

	return (promvec->pv_nodeops->no_nextnode(node));
}

/* The following recursively searches a PROM tree for a given node */
int
search_prom(rootnode, name)
        register int rootnode;
        register char *name;
{
        register int rtnnode;
        register int node = rootnode;

        if (node == findroot() || !strcmp("hierarchical",
                                          getpropstring(node, "device_type")))
            node = firstchild(node);

        if (!node)
            panic("search_prom: null node");

        do {
                if (strcmp(getpropstring(node, "name"),name) == 0)
                    return node;

                if (node_has_property(node,"device_type") &&
                    (!strcmp(getpropstring(node, "device_type"),"hierarchical")
                     || !strcmp(getpropstring(node, "name"),"iommu"))
                    && (rtnnode = search_prom(node, name)) != 0)
                        return rtnnode;

        } while ((node = nextsibling(node)));

        return 0;
}

/* The following are used primarily in consinit() */

int
opennode(path)		/* translate phys. device path to node */
	register char *path;
{
	register int fd;

	if (promvec->pv_romvec_vers < 2) {
		printf("WARNING: opennode not valid on sun4! %s\n", path);
		return (0);
	}
	fd = promvec->pv_v2devops.v2_open(path);
	if (fd == 0)
		return 0;
	return promvec->pv_v2devops.v2_fd_phandle(fd);
}

int
node_has_property(node, prop)	/* returns 1 if node has given property */
	register int node;
	register const char *prop;
{

	return ((*promvec->pv_nodeops->no_proplen)(node, (caddr_t)prop) != -1);
}

/* Pass a string to the FORTH PROM to be interpreted */
void
rominterpret(s)
	register char *s;
{

	if (promvec->pv_romvec_vers < 2)
		promvec->pv_fortheval.v0_eval(strlen(s), s);
	else
		promvec->pv_fortheval.v2_eval(s);
}

/*
 * Try to figure out where the PROM stores the cursor row & column
 * variables.  Returns nonzero on error.
 */
int
romgetcursoraddr(rowp, colp)
	register int **rowp, **colp;
{
	char buf[100];

	/*
	 * line# and column# are global in older proms (rom vector < 2)
	 * and in some newer proms.  They are local in version 2.9.  The
	 * correct cutoff point is unknown, as yet; we use 2.9 here.
	 */
	if (promvec->pv_romvec_vers < 2 || promvec->pv_printrev < 0x00020009)
		snprintf(buf, sizeof buf,
		    "' line# >body >user %lx ! ' column# >body >user %lx !",
		    (u_long)rowp, (u_long)colp);
	else
		snprintf(buf, sizeof buf,
		    "stdout @ is my-self addr line# %lx ! addr column# %lx !",
		    (u_long)rowp, (u_long)colp);
	*rowp = *colp = NULL;
	rominterpret(buf);
	return (*rowp == NULL || *colp == NULL);
}

void
romhalt()
{
	if (CPU_ISSUN4COR4M)
		*promvec->pv_synchook = NULL;

	promvec->pv_halt();
	panic("PROM exit failed");
}

void
romboot(str)
	char *str;
{
	if (CPU_ISSUN4COR4M)
		*promvec->pv_synchook = NULL;

	promvec->pv_reboot(str);
	panic("PROM boot failed");
}

void
callrom()
{

#if 0			/* sun4c FORTH PROMs do this for us */
	if (CPU_ISSUN4)
		fb_unblank();
#endif
	promvec->pv_abort();
}

/*
 * find the boot device (if it was a disk).   we must check to see if
 * unit info in saved bootpath structure matches unit info in our softc.
 * note that knowing the device name (e.g. "xd0") is not useful... we
 * must check the drive number (or target/lun, in the case of SCSI).
 * (XXX is it worth ifdef'ing this?)
 */

void
device_register(struct device *dev, void *aux)
{
	struct bootpath *bp = bootpath_store(0, NULL); /* restore bootpath! */

	if (bp == NULL)
		return;

	/*
	 * scsi: sd,cd
	 */
	if (strcmp("sd", dev->dv_cfdata->cf_driver->cd_name) == 0 ||
	    strcmp("cd", dev->dv_cfdata->cf_driver->cd_name) == 0) {
		struct scsi_attach_args *sa = aux;
		struct scsibus_softc *sbsc;
		int target, lun;

		sbsc = (struct scsibus_softc *)dev->dv_parent;

		target = bp->val[0];
		lun = bp->val[1];

#if defined(SUN4)
		if (CPU_ISSUN4 && dev->dv_xname[0] == 's' &&
		    target == 0 && sbsc->sc_link[0][0] == NULL) {
			/*
			 * disk unit 0 is magic: if there is actually no
			 * target 0 scsi device, the PROM will call
			 * target 3 `sd0'.
			 * XXX - what if someone puts a tape at target 0?
			 */
			/* Note that sc_link[0][0] will be NULL when we are
			 * invoked to match the device for target 0, if it
			 * exists. But then the attachment args will have
			 * its own target set to zero. It this case, skip
			 * the remapping.
			 */
			if (sa->sa_sc_link->target != 0) {
				target = 3;	/* remap to 3 */
				lun = 0;
			}
		}
#endif

#if defined(SUN4C)
		if (CPU_ISSUN4C && dev->dv_xname[0] == 's')
			target = sd_crazymap(target);
#endif

		if (sa->sa_sc_link->target == target &&
		    sa->sa_sc_link->lun == lun) {
			bp->dev = dev;	/* got it! */
			return;
		}
	}
}

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(name, unit)
	char *name;
	int unit;
{
	struct device *dev = TAILQ_FIRST(&alldevs);
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	snprintf(num, sizeof num, "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strlcpy(fullname, name, sizeof fullname);
	strlcat(fullname, num, sizeof fullname);

	while (strcmp(dev->dv_xname, fullname) != 0) {
		if ((dev = TAILQ_NEXT(dev, dv_list)) == NULL)
			return NULL;
	}
	return dev;
}

struct nam2blk nam2blk[] = {
	{ "xy",		 3 },
	{ "sd",		 7 },
	{ "xd",		10 },
	{ "st",		11 },
	{ "fd",		16 },
	{ "rd",		17 },
	{ "cd",		18 },
	{ "raid",	25 },
	{ "vnd",	8 },
	{ NULL,		-1 }
};
