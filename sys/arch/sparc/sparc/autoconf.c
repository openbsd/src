/*	$NetBSD: autoconf.c,v 1.38 1995/10/09 22:34:03 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
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
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

#include <sys/param.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#ifdef SUN4
#include <machine/oldmon.h>
#include <machine/idprom.h>
#include <sparc/sparc/memreg.h>
#endif
#include <machine/cpu.h>

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold;		/* if 1, still working on cold-start */
int	dkn;		/* number of iostat dk numbers assigned so far */
int	fbnode;		/* node ID of ROM's console frame buffer */
int	optionsnode;	/* node ID of ROM's options */
int	cpumod;		/* CPU model,
			 * XXX currently valid only if cputyp == CPU_SUN4
			 */
int	mmu_3l;		/* SUN4_400 models have a 3-level MMU */

extern	struct promvec *promvec;

static	int rootnode;
int	findroot __P((void));
void	setroot __P((void));
static	int getstr __P((char *, int));
static	int findblkmajor __P((struct device *));
static	struct device *getdisk __P((char *, int, int, dev_t *));

struct	bootpath bootpath[8];
static	void bootpath_build __P((void));
static	void bootpath_fake __P((struct bootpath *, char *));
static	void bootpath_print __P((struct bootpath *));

/*
 * Most configuration on the SPARC is done by matching OPENPROM Forth
 * device names with our internal names.
 */
int
matchbyname(parent, vcf, aux)
	struct device *parent;
	void *aux, *vcf;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;

#if defined(SUN4)
	if (cputyp == CPU_SUN4) {
		printf("WARNING: matchbyname not valid on sun4!");
		printf("%s\n", cf->cf_driver->cd_name);
		return (0);
	}
#endif
	return (strcmp(cf->cf_driver->cd_name, ca->ca_ra.ra_name) == 0);
}

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

struct idprom idprom;
void	getidprom __P((struct idprom *, int size));
#endif

/*
 * locore.s code calls bootstrap() just before calling main(), after double
 * mapping the kernel to high memory and setting up the trap base register.
 * We must finish mapping the kernel properly and glean any bootstrap info.
 */
void
bootstrap()
{
	int nregion, nsegment, ncontext, node;
#ifdef KGDB
	extern int kgdb_debug_panic;
#endif

#if defined(SUN4)
	extern void oldmon_w_cmd();
	extern struct msgbuf *msgbufp;

	if (cputyp == CPU_SUN4) {
		/*
		 * XXX
		 * Some boot programs mess up physical page 0, which
		 * is where we want to put the msgbuf. There's some
		 * room, so shift it over half a page.
		 */
		msgbufp = (struct msgbuf *)((caddr_t) msgbufp + 4096);

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
		getidprom(&idprom, sizeof(idprom));
		switch (cpumod = idprom.id_machine) {
		case SUN4_100:
			nsegment = 256;
			ncontext = 8;
			break;
		case SUN4_200:
			nsegment = 512;
			ncontext = 16;
			break;
		case SUN4_300:
			nsegment = 256;
			ncontext = 16;
			break;
		case SUN4_400:
			nsegment = 1024;
			ncontext = 64;
			nregion = 256;
			mmu_3l = 1;
			break;
		default:
			printf("bootstrap: sun4 machine type %2x unknown!\n",
			    idprom.id_machine);
			callrom();
		}
	}
#endif /* SUN4 */
#if defined(SUN4C) || defined(SUN4M)
	if (cputyp == CPU_SUN4C || cputyp == CPU_SUN4M) {
		node = findroot();
		nsegment = getpropint(node, "mmu-npmg", 128);
		ncontext = getpropint(node, "mmu-nctx", 8);
	}
#endif /* SUN4C || SUN4M */
	pmap_bootstrap(ncontext, nregion, nsegment);
#ifdef KGDB
	zs_kgdb_init();			/* XXX */
#endif
#ifdef DDB
	db_machine_init();
	ddb_init();
#endif
}


/*
 * bootpath_build: build a bootpath. Used when booting a generic
 * kernel to find our root device.  Newer proms give us a bootpath,
 * for older proms we have to create one.  An element in a bootpath
 * has 3 fields: name (device name), val[0], and val[1]. Note that:
 *
 * if (val[0] == -1) { 
 *	val[1] is a unit number    (happens most often with old proms)
 * } else {
 *	val[0] is a sbus slot, and val[1] is an sbus offset [if sbus]
 * }
 *
 */

static void 
bootpath_build()
{
	register char *cp, *pp;
	register struct bootpath *bp;
	int v0val[3];

	/*
	 * On SS1s, promvec->pv_v0bootargs->ba_argv[1] contains the flags
	 * that were given after the boot command.  On SS2s, pv_v0bootargs
	 * is NULL but *promvec->pv_v2bootargs.v2_bootargs points to
	 * "vmunix -s" or whatever.
	 * ###	DO THIS BEFORE pmap_boostrap?
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

		bootpath_print(bootpath);

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
			} else {
				bp->val[0] = -1; /* no #'s: assume unit 0, no
							sbus offset/adddress */
			}
#ifdef notyet
			if (strcmp(bp->name, "iommu") == 0)
				continue;
#endif
			++bp;
		}
		bp->name[0] = 0;

		bootpath_print(bootpath);

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

		case 'b':
			boothowto |= RB_DFLTROOT;
			break;

		case 'd':	/* kgdb - always on zs	XXX */
#ifdef KGDB
			boothowto |= RB_KDB;	/* XXX unused */
			kgdb_debug_panic = 1;
			kgdb_connect(1);
#else
			printf("kernel not compiled with KGDB\n");
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
 * The argument `cp' points to a string such as "xd(0,0,0)netbsd"
 */

static void
bootpath_fake(bp, cp)
	struct bootpath *bp;
	char *cp;
{
	register char *pp;
	int v0val[3];
	char tmpname[8];

#define BP_APPEND(BP,N,V0,V1) { \
	strcpy((BP)->name, N); \
	(BP)->val[0] = (V0); \
	(BP)->val[1] = (V1); \
	(BP)++; }


	pp = cp + 2;
	v0val[0] = v0val[1] = v0val[2] = 0;
	if (*pp == '(' 					/* for vi: ) */ 
 	    && *(pp = str2hex(++pp, &v0val[0])) == ',' 
	    && *(pp = str2hex(++pp, &v0val[1])) == ',')
		(void)str2hex(++pp, &v0val[2]);

#ifdef SUN4
	if (cputyp == CPU_SUN4) {

		/*
		 *  xylogics VME dev: xd, xy, xt 
		 *  fake looks like: /vmel0/xdc0/xd@1,0
		 */
		if (cp[0] == 'x') { 
			if (cp[1] == 'd') {/* xd? */
				BP_APPEND(bp,"vmel",-1,0);
			} else {
				BP_APPEND(bp,"vmes",-1,0);
			}
			sprintf(tmpname,"x%cc", cp[1]); /* e.g. xdc */
			BP_APPEND(bp,tmpname,-1,v0val[0]);
			sprintf(tmpname,"%c%c", cp[0], cp[1]);
			BP_APPEND(bp,tmpname,v0val[1], v0val[2]); /* e.g. xd */
			return;
		}

		/*
		 * ethernet: ie, le (rom supports only obio?)
		 * fake looks like: /obio0/le0
		 */
		if ((cp[0] == 'i' || cp[0] == 'l') && cp[1] == 'e')  {
			BP_APPEND(bp,"obio",-1,0);
			sprintf(tmpname,"%c%c", cp[0], cp[1]);
			BP_APPEND(bp,tmpname,-1,0);
			return;
		}

		/*
		 * scsi: sd, st, sr
		 * assume: 4/100 = sw: /obio0/sw0/sd@0,0
		 * 4/200 & 4/400 = si/sc: /vmes0/si0/sd@0,0
 		 * 4/300 = esp: /obio0/esp0/sd@0,0
		 * (note we expect sc to mimic an si...)
		 */
		if (cp[0] == 's' && 
			(cp[1] == 'd' || cp[1] == 't' || cp[1] == 'r')) {
			
			switch (cpumod) {
			case SUN4_200:
			case SUN4_400:
				BP_APPEND(bp,"vmes",-1,0);
				BP_APPEND(bp,"si",-1,v0val[0]);
				sprintf(tmpname,"%c%c", cp[0], cp[1]);
				BP_APPEND(bp,tmpname,v0val[1],v0val[2]);
				return;
			case SUN4_100:
				BP_APPEND(bp,"obio",-1,0);
				BP_APPEND(bp,"sw",-1,v0val[0]);
				sprintf(tmpname,"%c%c", cp[0], cp[1]);
				BP_APPEND(bp,tmpname,v0val[1],v0val[2]);
				return;
			case SUN4_300:
				BP_APPEND(bp,"obio",-1,0);
				BP_APPEND(bp,"esp",-1,v0val[0]);
				sprintf(tmpname,"%c%c", cp[0], cp[1]);
				BP_APPEND(bp,tmpname,v0val[1],v0val[2]);
				return;
			}
			panic("bootpath_fake: unknown cpumod?");
		}

		return; /* didn't grok bootpath, no change */
	}
#endif /* SUN4 */
		
#ifdef SUN4C
	/*
	 * sun4c stuff
	 */

	/*
	 * floppy: fd
	 * fake looks like: /fd@0,0
	 */
	if (cp[0] == 'f' && cp[1] == 'd') {
		BP_APPEND(bp,"fd",v0val[1],v0val[2]);
		return;
	}

	/*
	 * ethenet: le
	 * fake looks like: /sbus0/le0
	 */
	if (cp[0] == 'l' && cp[1] == 'e') {
		BP_APPEND(bp,"sbus",-1,0);
		BP_APPEND(bp,"le",-1,v0val[0]);
		return;
	}

	/*
	 * scsi: sd, st, sr
	 * fake looks like: /sbus0/esp0/sd@3,0
	 */		
	if (cp[0] == 's' &&
	    (cp[1] == 'd' || cp[1] == 't' || cp[1] == 'r')) {
		BP_APPEND(bp,"sbus",-1,0);
		BP_APPEND(bp,"esp",-1,v0val[0]);
		if (cp[1] == 'r') 
			sprintf(tmpname, "cd"); /* netbsd uses 'cd', not 'sr'*/
		else
			sprintf(tmpname,"%c%c", cp[0], cp[1]);
		BP_APPEND(bp,tmpname,v0val[1], v0val[2]);
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
		bp++;
	}
	printf("\n");
}


/*
 * save or read a bootpath pointer from the boothpath store.
 *
 * XXX. required because of SCSI... we don't have control over the "sd"
 * device, so we can't set boot device there.   we patch in with 
 * dk_establish(), and use this to recover the bootpath.
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
	struct nodeops *no;

	if (cputyp != CPU_SUN4 && promvec->pv_romvec_vers < 2) {
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
		 * and v[2-] OpenPROMs. Note: dkestablish() does the
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

int 
st_crazymap(n)
	int	n;
{
	static int prom_st_crazymap[8]; /* static: compute only once! */
	static int init = 0;

	if (init == 0) {
		crazymap("st-targets", prom_st_crazymap);
		init = 1;
	}
	return prom_st_crazymap[n];
}

/*
 * Determine mass storage and memory configuration for a machine.
 * We get the PROM's root device and make sure we understand it, then
 * attach it as `mainbus0'.  We also set up to handle the PROM `sync'
 * command.
 */
configure()
{
	struct confargs oca;
	register int node = 0;
	register char *cp;
	void sync_crash();

	/* build the bootpath */
	bootpath_build();

#if defined(SUN4)
	if (cputyp == CPU_SUN4) {
		extern struct cfdata cfdata[];
		extern struct cfdriver memregcd, obiocd;
		struct cfdata *cf, *memregcf = NULL;
		register short *p;

		for (cf = cfdata; memregcf==NULL && cf->cf_driver; cf++) {
			if (cf->cf_driver != &memregcd)
				continue;
			/*
			 * On the 4/100 obio addresses must be mapped at
			 * 0x0YYYYYYY, but alias higher up (we avoid the
			 * alias condition because it causes pmap difficulties)
			 * XXX: We also assume that 4/[23]00 obio addresses
			 * must be 0xZYYYYYYY, where (Z != 0)
			 * make sure we get the correct memreg cfdriver!
			 */
			if (cpumod==SUN4_100 && (cf->cf_loc[0] & 0xf0000000))
				continue;
			if (cpumod!=SUN4_100 && !(cf->cf_loc[0] & 0xf0000000))
				continue;
			for (p = cf->cf_parents; memregcf==NULL && *p >= 0; p++)
				if (cfdata[*p].cf_driver == &obiocd)
					memregcf = cf;
		}
		if (memregcf==NULL)
			panic("configure: no memreg found!");
		par_err_reg = (int *)bus_map(memregcf->cf_loc[0], NBPG, BUS_OBIO);
		if (par_err_reg == NULL)
			panic("configure: ROM hasn't mapped memreg!");
	}
#endif
#if defined(SUN4C) || defined(SUN4M)
	if (cputyp == CPU_SUN4C || cputyp == CPU_SUN4M) {
		node = findroot();
		cp = getpropstring(node, "device_type");
		if (strcmp(cp, "cpu") != 0)
			panic("PROM root device type = %s (need CPU)\n", cp);
		*promvec->pv_synchook = sync_crash;
	}
#endif

	oca.ca_ra.ra_node = node;
	oca.ca_ra.ra_name = cp = "mainbus";
	if (!config_rootfound(cp, (void *)&oca))
		panic("mainbus not configured");
	(void)spl0();

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	setroot();
	swapconf();
	cold = 0;
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
int n;
	static char buf[10];

	freq /= 1000;
	sprintf(buf, "%d", freq / 1000);
	freq %= 1000;
	if (freq) {
		freq += 1000;	/* now in 1000..1999 */
		p = buf + strlen(buf);
		sprintf(p, "%d", freq);
		*p = '.';	/* now buf = %d.%3d */
	}
	return (buf);
}

/* ARGSUSED */
static int
mbprint(aux, name)
	void *aux;
	char *name;
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
	register char *name;
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
	register int len;
	union { char regbuf[128]; struct rom_reg rr[RA_MAXREG]; } u;
	static const char pl[] = "property length";

	bzero(u.regbuf, sizeof u);
	len = getprop(node, "reg", (void *)u.regbuf, sizeof u.regbuf);
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

	rp->ra_vaddr = (caddr_t)getpropint(node, "address", 0);
	len = getprop(node, "address", (void *)rp->ra_vaddrs,
		      sizeof(rp->ra_vaddrs));
	if (len == -1)
		len = 0;
	if (len & 3) {
		printf("%s \"address\" %s = %d (need multiple of 4)\n",
		    cp, pl, len);
		len = 0;
	}
	rp->ra_nvaddrs = len >> 2;

	len = getprop(node, "intr", (void *)&rp->ra_intr, sizeof rp->ra_intr);
	if (len == -1)
		len = 0;
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
	}
	return (1);
}

int
mainbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

/*
 * Attach the mainbus.
 *
 * Our main job is to attach the CPU (the root node we got in configure())
 * and iterate down the list of `mainbus devices' (children of that node).
 * We also record the `node id' of the default frame buffer, if any.
 */
static void
mainbus_attach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	struct confargs oca, *ca = aux;
	register int node0, node;
	register const char *cp, *const *ssp, *sp;
#define L1A_HACK		/* XXX hack to allow L1-A during autoconf */
#ifdef L1A_HACK
	int nzs = 0, audio = 0;
#endif
	static const char *const oldmon_special[] = {
		"vmel",
		"vmes",
		NULL
	};

	static const char *const openboot_special[] = {
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

	printf("\n");

	/* configure the cpu */
	node = ca->ca_ra.ra_node;
	oca.ca_ra.ra_node = node;
	oca.ca_ra.ra_name = cp = "cpu";
	oca.ca_ra.ra_paddr = 0;
	oca.ca_ra.ra_nreg = 0;
	config_found(dev, (void *)&oca, mbprint);

	/*
	 * Locate and configure the ``early'' devices.  These must be
	 * configured before we can do the rest.  For instance, the
	 * EEPROM contains the Ethernet address for the LANCE chip.
	 * If the device cannot be located or configured, panic.
	 */
#if defined(SUN4)
	if (cputyp == CPU_SUN4) {

		/* Start at the beginning of the bootpath */
		oca.ca_ra.ra_bp = bootpath;

		oca.ca_bustype = BUS_MAIN;
		oca.ca_ra.ra_name = "obio";
		if (!config_found(dev, (void *)&oca, mbprint))
			panic(sp);

		for (ssp = oldmon_special; (sp = *ssp) != NULL; ssp++) {
			oca.ca_bustype = BUS_MAIN;
			oca.ca_ra.ra_name = sp;
			(void)config_found(dev, (void *)&oca, mbprint);
		}
	} else
#endif
	{

		/* remember which frame buffer, if any, is to be /dev/fb */
		fbnode = getpropint(node, "fb", 0);

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
			    !config_found(dev, (void *)&oca, mbprint))
				panic(sp);
		}

		/*
		 * Configure the rest of the devices, in PROM order.  Skip
		 * PROM entries that are not for devices, or which must be
		 * done before we get here.
		 */
		for (node = node0; node; node = nextsibling(node)) {
			cp = getpropstring(node, "name");
			for (ssp = openboot_special; (sp = *ssp) != NULL; ssp++)
				if (strcmp(cp, sp) == 0)
					break;
			if (sp == NULL && romprop(&oca.ca_ra, cp, node)) {
#ifdef L1A_HACK
				if (strcmp(cp, "audio") == 0)
					audio = 1;
				if (strcmp(cp, "zs") == 0)
					nzs++;
				if (audio && nzs >= 2)
					(void) splx(11 << 8);	/* XXX */
#endif
				oca.ca_bustype = BUS_MAIN;
				(void) config_found(dev, (void *)&oca, mbprint);
			}
		}
	}
}

struct cfdriver mainbuscd =
    { NULL, "mainbus", mainbus_match, mainbus_attach,
      DV_DULL, sizeof(struct device) };

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
	register int node, addr;

#ifdef SUN4
#define ZS0_PHYS	0xf1000000
#define ZS1_PHYS	0xf0000000
#define ZS2_PHYS	0xe0000000

	if (cputyp == CPU_SUN4) {
		void *paddr;

		switch (zs) {
		case 0:
			paddr = (void *)ZS0_PHYS;
			break;
		case 1:
			paddr = (void *)ZS1_PHYS;
			break;
		case 2:
			paddr = (void *)ZS2_PHYS;
			break;
		default:
			panic("findzs: unknown zs device %d", zs);
		}

		addr = bus_map(paddr, NBPG, BUS_OBIO);
		if (addr)
			return ((void *)addr);
	}
#endif
#if defined(SUN4C) || defined(SUN4M)
	if (cputyp == CPU_SUN4C || cputyp == CPU_SUN4M) {
		node = firstchild(findroot());
		while ((node = findnode(node, "zs")) != 0) {
			if (getpropint(node, "slave", -1) == zs) {
				if ((addr = getpropint(node, "address", 0)) == 0)
					panic("findzs: zs%d not mapped by PROM", zs);
				return ((void *)addr);
			}
			node = nextsibling(node);
		}
	}
#endif
bail:
	panic("findzs: cannot find zs%d", zs);
	/* NOTREACHED */
}

int
makememarr(ap, max, which)
	register struct memarr *ap;
	int max, which;
{
#if defined(SUN4C) || defined(SUN4M)
	struct v2rmi {
		int	zero;
		int	addr;
		int	len;
	} v2rmi[200];		/* version 2 rom meminfo layout */
#define	MAXMEMINFO (sizeof(v2rmi) / sizeof(*v2rmi))
	register struct v0mlist *mp;
	register int i, node, len;
	char *prop;
#endif

#if defined(SUN4)
	if (cputyp == CPU_SUN4) {
		switch (which) {
		case MEMARR_AVAILPHYS:
			ap[0].addr = 0;
			ap[0].len = *oldpvec->memoryAvail;
			break;
		case MEMARR_TOTALPHYS:
			ap[0].addr = 0;
			ap[0].len = *oldpvec->memorySize;
			break;
		default:
			printf("pre_panic: makememarr");
			break;
		}
		return (1);
	}
#endif
#if defined(SUN4C) || defined(SUN4M)
	switch (i = promvec->pv_romvec_vers) {

	case 0:
		/*
		 * Version 0 PROMs use a linked list to describe these
		 * guys.
		 */
		switch (which) {

		case MEMARR_AVAILPHYS:
			mp = *promvec->pv_v0mem.v0_physavail;
			break;

		case MEMARR_TOTALPHYS:
			mp = *promvec->pv_v0mem.v0_phystot;
			break;

		default:
			panic("makememarr");
		}
		for (i = 0; mp != NULL; mp = mp->next, i++) {
			if (i >= max)
				goto overflow;
			ap->addr = (u_int)mp->addr;
			ap->len = mp->nbytes;
			ap++;
		}
		break;

	default:
		printf("makememarr: hope version %d PROM is like version 2\n",
		    i);
		/* FALLTHROUGH */

	case 2:
		/*
		 * Version 2 PROMs use a property array to describe them.
		 */
		if (max > MAXMEMINFO) {
			printf("makememarr: limited to %d\n", MAXMEMINFO);
			max = MAXMEMINFO;
		}
		if ((node = findnode(firstchild(findroot()), "memory")) == 0)
			panic("makememarr: cannot find \"memory\" node");
		switch (which) {

		case MEMARR_AVAILPHYS:
			prop = "available";
			break;

		case MEMARR_TOTALPHYS:
			prop = "reg";
			break;

		default:
			panic("makememarr");
		}
		len = getprop(node, prop, (void *)v2rmi, sizeof v2rmi) /
		    sizeof(struct v2rmi);
		for (i = 0; i < len; i++) {
			if (i >= max)
				goto overflow;
			ap->addr = v2rmi[i].addr;
			ap->len = v2rmi[i].len;
			ap++;
		}
		break;
	}

	/*
	 * Success!  (Hooray)
	 */
	if (i == 0)
		panic("makememarr: no memory found");
	return (i);

overflow:
	/*
	 * Oops, there are more things in the PROM than our caller
	 * provided space for.  Truncate any extras.
	 */
	printf("makememarr: WARNING: lost some memory\n");
	return (i);
#endif
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
	register struct nodeops *no;
	register int len;

#if defined(SUN4)
	if (cputyp==CPU_SUN4) {
		printf("WARNING: getprop not valid on sun4! %s\n", name);
		return (0);
	}
#endif
#if defined(SUN4C) || defined(SUN4M)
	no = promvec->pv_nodeops;
	len = no->no_proplen(node, name);
	if (len > bufsiz) {
		printf("node %x property %s length %d > %d\n",
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

#ifdef RASTERCONSOLE
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
		sprintf(buf,
		    "' line# >body >user %x ! ' column# >body >user %x !",
		    rowp, colp);
	else
		sprintf(buf,
		    "stdout @ is my-self addr line# %x ! addr column# %x !",
		    rowp, colp);
	*rowp = *colp = NULL;
	rominterpret(buf);
	return (*rowp == NULL || *colp == NULL);
}
#endif

volatile void
romhalt()
{

	promvec->pv_halt();
	panic("PROM exit failed");
}

volatile void
romboot(str)
	char *str;
{

	promvec->pv_reboot(str);
	panic("PROM boot failed");
}

callrom()
{

#if 0			/* sun4c FORTH PROMs do this for us */
	if (cputyp == CPU_SUN4)
		fb_unblank();
#endif
	promvec->pv_abort();
}

/*
 * Configure swap space and related parameters.
 */
swapconf()
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++)
		if (bdevsw[major(swp->sw_dev)].d_psize) {
			nblks =
			  (*bdevsw[major(swp->sw_dev)].d_psize)(swp->sw_dev);
			if (nblks != -1 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	dumpconf();
}

#if 0 /* Unused */
dev_t	bootdev;
#endif

#define	PARTITIONMASK	0x7
#define	PARTITIONSHIFT	3

struct nam2blk {
	char *name;
	int maj;
} nam2blk[] = {
	{ "xy",		3 },
	{ "sd",		7 },
	{ "xd",		10 },
	{ "st",		11 },
	{ "fd",		16 },
	{ "cd",		18 },
};

static int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	register int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[0].name)) == 0)
			return (nam2blk[i].maj);
	return (-1);
}

static struct device *
getdisk(str, len, defpart, devp)
	char *str;
	int len, defpart;
	dev_t *devp;
{
	register struct device *dv;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of:");
		for (dv = alldevs; dv != NULL; dv = dv->dv_next) {
			if (dv->dv_class == DV_DISK)
				printf(" %s[a-h]", dv->dv_xname);
#ifdef NFSCLIENT
			if (dv->dv_class == DV_IFNET)
				printf(" %s", dv->dv_xname);
#endif
		}
		printf("\n");
	}
	return (dv);
}

struct device *
parsedisk(str, len, defpart, devp)
	char *str;
	int len, defpart;
	dev_t *devp;
{
	register struct device *dv;
	register char *cp, c;
	int majdev, mindev, part;

	if (len == 0)
		return (NULL);
	cp = str + len - 1;
	c = *cp;
	if (c >= 'a' && c <= 'h') {
		part = c - 'a';
		*cp = '\0';
	} else
		part = defpart;

	for (dv = alldevs; dv != NULL; dv = dv->dv_next) {
		if (dv->dv_class == DV_DISK &&
		    strcmp(str, dv->dv_xname) == 0) {
			majdev = findblkmajor(dv);
			if (majdev < 0)
				panic("parsedisk");
			mindev = (dv->dv_unit << PARTITIONSHIFT) + part;
			*devp = makedev(majdev, mindev);
			break;
		}
#ifdef NFSCLIENT
		if (dv->dv_class == DV_IFNET &&
		    strcmp(str, dv->dv_xname) == 0) {
			*devp = NODEV;
			break;
		}
#endif
	}

	*cp = c;
	return (dv);
}

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 * 
 * XXX Actually, swap and root must be on the same type of device,
 * (ie. DV_DISK or DV_IFNET) because of how (*mountroot) is written.
 * That should be fixed.
 */
void
setroot()
{
	register struct swdevt *swp;
	register struct device *dv;
	register int len, majdev, mindev;
	dev_t nrootdev, nswapdev = NODEV;
	char buf[128];
	extern int (*mountroot)();
	dev_t temp;
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
	extern int nfs_mountroot();
#endif
#if defined(FFS)
	extern int ffs_mountroot();
#endif

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device ");
			if (bootdv != NULL)
				printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK?'a':' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				strcpy(buf, bootdv->dv_xname);
				len = strlen(buf);
			}
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				dv = getdisk(buf, len, 1, &nrootdev);
				if (dv != NULL) {
					bootdv = dv;
					nswapdev = nrootdev;
					goto gotswap;
				}
			}
			dv = getdisk(buf, len, 0, &nrootdev);
			if (dv != NULL) {
				bootdv = dv;
				break;
			}
		}

		/*
		 * because swap must be on same device as root, for
		 * network devices this is easy.
		 */
		if (bootdv->dv_class == DV_IFNET) {
			goto gotswap;
		}
		for (;;) {
			printf("swap device ");
			if (bootdv != NULL)
				printf("(default %s%c)",
					bootdv->dv_xname,
					bootdv->dv_class == DV_DISK?'b':' ');
			printf(": ");
			len = getstr(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				switch (bootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = makedev(major(nrootdev),
					    (minor(nrootdev) & ~ PARTITIONMASK) | 1);
					break;
				}
				break;
			}
			dv = getdisk(buf, len, 1, &nswapdev);
			if (dv) {
				if (dv->dv_class == DV_IFNET)
					nswapdev = NODEV;
				break;
			}
		}
gotswap:
		rootdev = nrootdev;
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		swdevt[1].sw_dev = NODEV;

	} else if (mountroot == NULL) {

		/*
		 * `swap generic': Use the device the ROM told us to use.
		 */
		if (bootdv == NULL)
			panic("boot device not known");

		majdev = findblkmajor(bootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on a disk.
			 * Assume that we are supposed to put root on
			 * partition a, and swap on partition b.
			 */
			mindev = (bootdv->dv_unit << PARTITIONSHIFT) + 0;
			rootdev = makedev(majdev, mindev);
			nswapdev = dumpdev = makedev(major(rootdev),
			    (minor(rootdev) & ~ PARTITIONMASK) | 1);
		} else {
			/*
			 * Root and swap are on a net.
			 */
			nswapdev = dumpdev = NODEV;
		}
		swdevt[0].sw_dev = nswapdev;
		swdevt[1].sw_dev = NODEV;

	} else {

		/*
		 * `root DEV swap DEV': honour rootdev/swdevt.
		 * rootdev/swdevt/mountroot already properly set.
		 */
		return;
	}

	switch (bootdv->dv_class) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = bootdv->dv_xname;
		return;
#endif
#if defined(FFS)
	case DV_DISK:
		mountroot = ffs_mountroot;
		majdev = major(rootdev);
		mindev = minor(rootdev);
		printf("root on %s%c\n", bootdv->dv_xname,
		    (mindev & PARTITIONMASK) + 'a');
		break;
#endif
	default:
		printf("can't figure root, hope your kernel is right\n");
		return;
	}

	/*
	 * XXX: What is this doing?
	 */
	mindev &= ~PARTITIONMASK;
	temp = NODEV;
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    mindev == (minor(swp->sw_dev) & ~PARTITIONMASK)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == NODEV)
		return;

	/*
	 * If dumpdev was the same as the old primary swap device, move
	 * it to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
}

static int
getstr(cp, size)
	register char *cp;
	register int size;
{
	register char *lp;
	register int c;
	register int len;

	lp = cp;
	len = 0;
	for (;;) {
		c = cngetc();
		switch (c) {
		case '\n':
		case '\r':
			printf("\n");
			*lp++ = '\0';
			return (len);
		case '\b':
		case '\177':
		case '#':
			if (len) {
				--len;
				--lp;
				printf("\b \b");
			}
			continue;
		case '@':
		case 'u'&037:
			len = 0;
			lp = cp;
			printf("\n");
			continue;
		default:
			if (len + 1 >= size || c < ' ') {
				printf("\007");
				continue;
			}
			printf("%c", c);
			++len;
			*lp++ = c;
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
	struct device *dev = alldevs;
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	sprintf(num, "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strcpy(fullname, name);
	strcat(fullname, num);

	while (strcmp(dev->dv_xname, fullname) != 0) {
		if ((dev = dev->dv_next) == NULL)
			return NULL;
	}
	return dev;
}
