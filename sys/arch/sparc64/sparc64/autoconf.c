/*	$OpenBSD: autoconf.c,v 1.71 2007/10/17 21:23:28 kettenis Exp $	*/
/*	$NetBSD: autoconf.c,v 1.51 2001/07/24 19:32:11 eeh Exp $ */

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
#include <sys/msgbuf.h>

#include <net/if.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>
#include <machine/sparc64.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <sparc64/sparc64/timerreg.h>

#include <dev/ata/atavar.h>
#include <dev/pci/pcivar.h>
#include <dev/sbus/sbusvar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

int printspl = 0;

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	stdinnode;	/* node ID of ROM's console input device */
int	fbnode;		/* node ID of ROM's console output device */
int	optionsnode;	/* node ID of ROM's options */

#ifdef KGDB
extern	int kgdb_debug_panic;
#endif

static	int rootnode;
char platform_type[64];

/* for hw.product/vendor see sys/kern/kern_sysctl.c */
extern char *hw_prod, *hw_vendor;

static	char *str2hex(char *, long *);
static	int mbprint(void *, const char *);
void	sync_crash(void);
int	mainbus_match(struct device *, void *, void *);
static	void mainbus_attach(struct device *, struct device *, void *);
int	get_ncpus(void);

struct device *booted_device;
struct	bootpath bootpath[8];
int	nbootpath;
int	bootnode;
static	void bootpath_build(void);
static	void bootpath_print(struct bootpath *);
void bootpath_nodes(struct bootpath *, int);

int bus_class(struct device *);
int instance_match(struct device *, void *, struct bootpath *bp);
void nail_bootdev(struct device *, struct bootpath *);

/* Global interrupt mappings for all device types.  Match against the OBP
 * 'device_type' property. 
 */
struct intrmap intrmap[] = {
	{ "block",	PIL_FD },	/* Floppy disk */
	{ "serial",	PIL_SER },	/* zs */
	{ "scsi",	PIL_SCSI },
	{ "scsi-2",	PIL_SCSI },
	{ "network",	PIL_NET },
	{ "display",	PIL_VIDEO },
	{ "audio",	PIL_AUD },
	{ "ide",	PIL_SCSI },
/* The following devices don't have device types: */
	{ "SUNW,CS4231",	PIL_AUD },
	{ NULL,		0 }
};


#ifdef DEBUG
#define ACDB_BOOTDEV	0x1
#define	ACDB_PROBE	0x2
int autoconf_debug = 0x0;
#define DPRINTF(l, s)   do { if (autoconf_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

/*
 * Convert hex ASCII string to a value.  Returns updated pointer.
 * Depends on ASCII order (this *is* machine-dependent code, you know).
 */
static char *
str2hex(char *str, long *vp)
{
	long v;
	int c;

	if (*str == 'w') {
		for (v = 1;; v++) {
			if (str[v] >= '0' && str[v] <= '9')
				continue;
			if (str[v] >= 'a' && str[v] <= 'f')
				continue;
			if (str[v] >= 'A' && str[v] <= 'F')
				continue;
			if (str[v] == '\0' || str[v] == ',')
				break;
			*vp = 0;
			return (str + v);
		}
		str++;
	}

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

int
get_ncpus(void)
{
#ifdef MULTIPROCESSOR
	int node0, node,ncpus;
	char buf[32];

	node = findroot();

	ncpus = 0;
	for (node = OF_child(node), node0 = 0; node; node = OF_peer(node)) {
		/* 
		 * UltraSPARC-IV cpus appear as two "cpu" nodes below
		 * a "cmp" node.  Go down one level, but remember
		 * where we came from, such that we can go up again
		 * after we've handled both "cpu" nodes.
		 */
		if (OF_getprop(node, "name", buf, sizeof(buf)) <= 0)
			continue;
		if (strcmp(buf, "cmp") == 0) {
			node0 = node;
			node = OF_child(node0);
		}

		if (OF_getprop(node, "device_type", buf, sizeof(buf)) <= 0)
			continue;
		if (strcmp(buf, "cpu") == 0)
			ncpus++;

		if (node0 && OF_peer(node) == 0) {
			node = node0;
			node0 = 0;
		}
	}

	return (ncpus);
#else
	return (1);
#endif
}
/*
 * locore.s code calls bootstrap() just before calling main().
 *
 * What we try to do is as follows:
 *
 * 1) We will try to re-allocate the old message buffer.
 *
 * 2) We will then get the list of the total and available
 *	physical memory and available virtual memory from the
 *	prom.
 *
 * 3) We will pass the list to pmap_bootstrap to manage them.
 *
 * We will try to run out of the prom until we get to cpu_init().
 */
void
bootstrap(nctx)
	int nctx;
{
	extern int end;	/* End of kernel */
	int ncpus;

	/* 
	 * Initialize ddb first and register OBP callbacks.
	 * We can do this because ddb_init() does not allocate anything,
	 * just initializes some pointers to important things
	 * like the symtab.
	 *
	 * By doing this first and installing the OBP callbacks
	 * we get to do symbolic debugging of pmap_bootstrap().
	 */
#ifdef KGDB
/* Moved zs_kgdb_init() to dev/zs.c:consinit(). */
	zs_kgdb_init();		/* XXX */
#endif
	/* Initialize the PROM console so printf will not panic */
	(*cn_tab->cn_init)(cn_tab);
#ifdef DDB
	db_machine_init();
	ddb_init();
	/* This can only be installed on an 64-bit system cause otherwise our stack is screwed */
	OF_set_symbol_lookup(OF_sym2val, OF_val2sym);
#endif

	ncpus = get_ncpus();
	pmap_bootstrap(KERNBASE, (u_long)&end, nctx, ncpus);
}

void
bootpath_nodes(struct bootpath *bp, int nbp)
{
	int chosen;
	int i;
	char buf[128], *cp, c;

	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "bootpath", buf, sizeof(buf));
	cp = buf;

	for (i = 0; i < nbp; i++, bp++) {
		if (*cp == '\0')
			return;
		while (*cp != '\0' && *cp == '/')
			cp++;
		while (*cp && *cp != '/')
			cp++;
		c = *cp;
		*cp = '\0';
		bootnode = bp->node = OF_finddevice(buf);
		*cp = c;
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
 *	[pci device] val[0] is device, val[1] is function, val[2] might be partition
 * }
 *
 */

static void
bootpath_build()
{
	register char *cp, *pp;
	register struct bootpath *bp;
	register long chosen;
	char buf[128];

	bzero((void *)bootpath, sizeof(bootpath));
	bp = bootpath;

	/*
	 * Grab boot path from PROM
	 */
	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "bootpath", buf, sizeof(buf));
	cp = buf;
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
				 * We only store one character here, as we will
				 * only use this field to compute a partition
				 * index for block devices.  However, it might
				 * be an ethernet media specification, so be
				 * sure to skip all letters.
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
	
	bootpath_nodes(bootpath, nbootpath);
	
	/* Setup pointer to boot flags */
	OF_getprop(chosen, "bootargs", buf, sizeof(buf));
	cp = buf;

	/* Find start of boot flags */
	while (*cp) {
		while(*cp == ' ' || *cp == '\t') cp++;
		if (*cp == '-' || *cp == '\0')
			break;
		while(*cp != ' ' && *cp != '\t' && *cp != '\0') cp++;
		
	}
	if (*cp != '-')
		return;

	for (;*++cp;) {
		int fl;

		fl = 0;
		switch(*cp) {
		case 'a':
			fl |= RB_ASKNAME;
			break;
		case 'b':
			fl |= RB_HALT;
			break;
		case 'c':
			fl |= RB_CONFIG;
			break;
		case 'd':
			fl |= RB_KDB;
			break;
		case 's':
			fl |= RB_SINGLE;
			break;
		default:
			break;
		}
		if (!fl) {
			printf("unknown option `%c'\n", *cp);
			continue;
		}
		boothowto |= fl;

		/* specialties */
		if (*cp == 'd') {
#if defined(KGDB)
			kgdb_debug_panic = 1;
			kgdb_connect(1);
#elif defined(DDB)
			Debugger();
#else
			printf("kernel has no debugger\n");
#endif
		} else if (*cp == 't') {
			/* turn on traptrace w/o breaking into kdb */
			extern int trap_trace_dis;

			trap_trace_dis = 0;
		}
	}
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
			printf("/%s@%lx,%lx", bp->name, bp->val[0], bp->val[1]);
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
 * Determine mass storage and memory configuration for a machine.
 * We get the PROM's root device and make sure we understand it, then
 * attach it as `mainbus0'.  We also set up to handle the PROM `sync'
 * command.
 */
void
cpu_configure()
{
#if 0
	extern struct user *proc0paddr;	/* XXX see below */
#endif

	/* build the bootpath */
	bootpath_build();

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}

#if notyet
        /* FIXME FIXME FIXME  This is probably *WRONG!!!**/
        OF_set_callback(sync_crash);
#endif

	/* block clock interrupts and anything below */
	splclock();
	/* Enable device interrupts */
        setpstate(getpstate()|PSTATE_IE);

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("mainbus not configured");

	/* Enable device interrupts */
        setpstate(getpstate()|PSTATE_IE);

#if 0
	/*
	 * XXX Re-zero proc0's user area, to nullify the effect of the
	 * XXX stack running into it during auto-configuration.
	 * XXX - should fix stack usage.
	 */
	bzero(proc0paddr, sizeof(struct user));
#endif

	(void)spl0();
	cold = 0;
}

void
diskconf(void)
{
	struct bootpath *bp;
	struct device *bootdv;

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
	long freq;
{
	char *p;
	static char buf[10];

	freq /= 1000;
	snprintf(buf, sizeof buf, "%ld", freq / 1000);
	freq %= 1000;
	if (freq) {
		freq += 1000;	/* now in 1000..1999 */
		p = buf + strlen(buf);
		snprintf(p, buf + sizeof buf - p, "%ld", freq);
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
	struct mainbus_attach_args *ma = aux;

	if (name)
		printf("\"%s\" at %s", ma->ma_name, name);
	if (ma->ma_address)
		printf(" addr 0x%08lx", (u_long)ma->ma_address[0]);
	if (ma->ma_pri)
		printf(" ipl %d", ma->ma_pri);
	return (UNCONF);
}

int
findroot()
{
	register int node;

	if ((node = rootnode) == 0 && (node = OF_peer(0)) == 0)
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
	int node;
	char buf[32];

	for (node = first; node; node = OF_peer(node)) {
		if ((OF_getprop(node, "name", buf, sizeof(buf)) > 0) &&
			(strcmp(buf, name) == 0))
			return (node);
	}
	return (0);
}

int
mainbus_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{

	return (1);
}

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
extern struct sparc_bus_dma_tag mainbus_dma_tag;
extern bus_space_tag_t mainbus_space_tag;

	struct mainbus_attach_args ma;
	char buf[32], *p;
	const char *const *ssp, *sp = NULL;
	int node0, node, rv, len, ncpus;

	static const char *const openboot_special[] = {
		/* ignore these (end with NULL) */
		/*
		 * These are _root_ devices to ignore. Others must be handled
		 * elsewhere.
		 */
		"virtual-memory",
		"aliases",
		"memory",
		"openprom",
		"options",
		"packages",
		"chosen",
		NULL
	};

	if ((len = OF_getprop(findroot(), "banner-name", platform_type,
	    sizeof(platform_type))) <= 0)
		OF_getprop(findroot(), "name", platform_type,
		    sizeof(platform_type));
	printf(": %s\n", platform_type);

	hw_vendor = malloc(sizeof(platform_type), M_DEVBUF, M_NOWAIT);
	if (len > 0 && hw_vendor != NULL) {
		strlcpy(hw_vendor, platform_type, sizeof(platform_type));
		if ((strncmp(hw_vendor, "SUNW,", 5)) == 0) {
			p = hw_prod = hw_vendor + 5;
			hw_vendor = "Sun";
		} else if ((p = memchr(hw_vendor, ' ', len)) != NULL) {
			*p = '\0';
			hw_prod = ++p;
		}
		if ((p = memchr(hw_prod, '(', len - (p - hw_prod))) != NULL)
			*p = '\0';
	}

	/* Establish the first component of the boot path */
	bootpath_store(1, bootpath);

	/* We configure the CPUs first. */

	node = findroot();

	ncpus = 0;
	for (node = OF_child(node), node0 = 0; node; node = OF_peer(node)) {
		/* 
		 * UltraSPARC-IV cpus appear as two "cpu" nodes below
		 * a "cmp" node.  Go down one level, but remember
		 * where we came from, such that we can go up again
		 * after we've handled both "cpu" nodes.
		 */
		if (OF_getprop(node, "name", buf, sizeof(buf)) <= 0)
			continue;
		if (strcmp(buf, "cmp") == 0) {
			node0 = node;
			node = OF_child(node0);
		}

		if (OF_getprop(node, "device_type", buf, sizeof(buf)) <= 0)
			continue;
		if (strcmp(buf, "cpu") == 0) {
			bzero(&ma, sizeof(ma));
			ma.ma_bustag = mainbus_space_tag;
			ma.ma_dmatag = &mainbus_dma_tag;
			ma.ma_node = node;
			ma.ma_name = "cpu";
			config_found(dev, (void *)&ma, mbprint);
			ncpus++;
		}

		if (node0 && OF_peer(node) == 0) {
			node = node0;
			node0 = 0;
		}
	}

	if (ncpus == 0)
		panic("None of the CPUs found");

	node = findroot();	/* re-init root node */

	/* Find the "options" node */
	node0 = OF_child(node);
	optionsnode = findnode(node0, "options");
	if (optionsnode == 0)
		panic("no options in OPENPROM");

	/*
	 * Configure the devices, in PROM order.  Skip
	 * PROM entries that are not for devices, or which must be
	 * done before we get here.
	 */
	for (node = node0; node; node = OF_peer(node)) {
		int portid;

		DPRINTF(ACDB_PROBE, ("Node: %x", node));
		if ((OF_getprop(node, "device_type", buf, sizeof(buf)) > 0) &&
			strcmp(buf, "cpu") == 0)
			continue;
		OF_getprop(node, "name", buf, sizeof(buf));
		DPRINTF(ACDB_PROBE, (" name %s\n", buf));
		for (ssp = openboot_special; (sp = *ssp) != NULL; ssp++)
			if (strcmp(buf, sp) == 0)
				break;
		if (sp != NULL)
			continue; /* an "early" device already configured */

		bzero(&ma, sizeof ma);
		ma.ma_bustag = mainbus_space_tag;
		ma.ma_dmatag = &mainbus_dma_tag;
		ma.ma_name = buf;
		ma.ma_node = node;
		if (OF_getprop(node, "upa-portid", &portid, sizeof(portid)) !=
		    sizeof(portid)) {
			if (OF_getprop(node, "portid", &portid,
			    sizeof(portid)) != sizeof(portid))
				portid = -1;
		}
		ma.ma_upaid = portid;

		if (getprop(node, "reg", sizeof(*ma.ma_reg), 
			     &ma.ma_nreg, (void **)&ma.ma_reg) != 0)
			continue;
#ifdef DEBUG
		if (autoconf_debug & ACDB_PROBE) {
			if (ma.ma_nreg)
				printf(" reg %08lx.%08lx\n",
					(long)ma.ma_reg->ur_paddr, 
					(long)ma.ma_reg->ur_len);
			else
				printf(" no reg\n");
		}
#endif
		rv = getprop(node, "interrupts", sizeof(*ma.ma_interrupts), 
			&ma.ma_ninterrupts, (void **)&ma.ma_interrupts);
		if (rv != 0 && rv != ENOENT) {
			free(ma.ma_reg, M_DEVBUF);
			continue;
		}
#ifdef DEBUG
		if (autoconf_debug & ACDB_PROBE) {
			if (ma.ma_interrupts)
				printf(" interrupts %08x\n", 
					*ma.ma_interrupts);
			else
				printf(" no interrupts\n");
		}
#endif
		rv = getprop(node, "address", sizeof(*ma.ma_address), 
			&ma.ma_naddress, (void **)&ma.ma_address);
		if (rv != 0 && rv != ENOENT) {
			free(ma.ma_reg, M_DEVBUF);
			if (ma.ma_ninterrupts)
				free(ma.ma_interrupts, M_DEVBUF);
			continue;
		}
#ifdef DEBUG
		if (autoconf_debug & ACDB_PROBE) {
			if (ma.ma_naddress)
				printf(" address %08x\n", 
					*ma.ma_address);
			else
				printf(" no address\n");
		}
#endif
		(void) config_found(dev, (void *)&ma, mbprint);
		free(ma.ma_reg, M_DEVBUF);
		if (ma.ma_ninterrupts)
			free(ma.ma_interrupts, M_DEVBUF);
		if (ma.ma_naddress)
			free(ma.ma_address, M_DEVBUF);
	}
	/* Try to attach PROM console */
	bzero(&ma, sizeof ma);
	ma.ma_name = "pcons";
	(void) config_found(dev, (void *)&ma, mbprint);
}

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

int
getprop(node, name, size, nitem, bufp)
	int	node;
	char	*name;
	size_t	size;
	int	*nitem;
	void	**bufp;
{
	void	*buf;
	long	len;

	*nitem = 0;
	len = getproplen(node, name);
	if (len <= 0)
		return (ENOENT);

	if ((len % size) != 0)
		return (EINVAL);

	buf = *bufp;
	if (buf == NULL) {
		/* No storage provided, so we allocate some */
		buf = malloc(len, M_DEVBUF, M_NOWAIT);
		if (buf == NULL)
			return (ENOMEM);
	}

	OF_getprop(node, name, buf, len);
	*bufp = buf;
	*nitem = len / size;
	return (0);
}


/*
 * Internal form of proplen().  Returns the property length.
 */
long
getproplen(node, name)
	int node;
	char *name;
{
	return (OF_getproplen(node, name));
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
	static char stringbuf[32];

	return (getpropstringA(node, name, stringbuf));
}

/* Alternative getpropstring(), where caller provides the buffer */
char *
getpropstringA(node, name, buffer)
	int node;
	char *name;
	char *buffer;
{
	int blen;

	if (getprop(node, name, 1, &blen, (void **)&buffer) != 0)
		blen = 0;

	buffer[blen] = '\0';	/* usually unnecessary */
	return (buffer);
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
	int intbuf;

	

	if (OF_getprop(node, name, &intbuf, sizeof(intbuf)) != sizeof(intbuf))
		return (deflt);

	return (intbuf);
}

/*
 * OPENPROM functions.  These are here mainly to hide the OPENPROM interface
 * from the rest of the kernel.
 */
int
firstchild(node)
	int node;
{

	return OF_child(node);
}

int
nextsibling(node)
	int node;
{

	return OF_peer(node);
}

/* The following are used primarily in consinit() */

int
node_has_property(node, prop)	/* returns 1 if node has given property */
	register int node;
	register const char *prop;
{
	return (OF_getproplen(node, (caddr_t)prop) != -1);
}

/*
 * Try to figure out where the PROM stores the cursor row & column
 * variables.  Returns nonzero on error.
 */
int
romgetcursoraddr(rowp, colp)
	int **rowp, **colp;
{
	cell_t row = NULL, col = NULL;

	OF_interpret("stdout @ is my-self addr line# addr column# ",
	    2, &col, &row);

	/*
	 * We are running on a 64-bit machine, so these things point to
	 * 64-bit values.  To convert them to pointers to interfaces, add
	 * 4 to the address.
	 */
	if (row == NULL || col == NULL)
		return (-1);
	*rowp = (int *)(row + 4);
	*colp = (int *)(col + 4);
	return (0);
}

void
callrom()
{

	__asm __volatile("wrpr	%%g0, 0, %%tl" : );
	OF_enter();
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

void
device_register(struct device *dev, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	struct pci_attach_args *pa = aux;
	struct sbus_attach_args *sa = aux;
	struct bootpath *bp = bootpath_store(0, NULL);
	struct device *busdev = dev->dv_parent;
	const char *devname = dev->dv_cfdata->cf_driver->cd_name;
	const char *busname;
	int node = -1;

	/*
	 * There is no point in continuing if we've exhausted all
	 * bootpath components.
	 */
	if (bp == NULL)
		return;

	DPRINTF(ACDB_BOOTDEV,
	    ("\n%s: device_register: devname %s(%s) component %s\n",
	    dev->dv_xname, devname, dev->dv_xname, bp->name));

	/*
	 * Ignore mainbus0 itself, it certainly is not a boot device.
	 */
	if (busdev == NULL)
		return;

	/*
	 * We don't know the type of 'aux'; it depends on the bus this
	 * device attaches to.  We are only interested in certain bus
	 * types; this is only used to find the boot device.
	 */
	busname = busdev->dv_cfdata->cf_driver->cd_name;
	if (strcmp(busname, "mainbus") == 0 || strcmp(busname, "upa") == 0)
		node = ma->ma_node;
	else if (strcmp(busname, "sbus") == 0 ||
	    strcmp(busname, "dma") == 0 || strcmp(busname, "ledma") == 0)
		node = sa->sa_node;
	else if (strcmp(busname, "pci") == 0)
		node = PCITAG_NODE(pa->pa_tag);

	if (node == bootnode) {
		nail_bootdev(dev, bp);
		return;
	}

	if (node == bp->node) {
		bp->dev = dev;
		DPRINTF(ACDB_BOOTDEV, ("\t-- matched component %s to %s\n",
		    bp->name, dev->dv_xname));
		bootpath_store(1, bp + 1);
		return;
	}

	if (strcmp(devname, "scsibus") == 0) {
		struct scsi_link *sl = aux;

		if (strcmp(bp->name, "fp") == 0 &&
		    bp->val[0] == sl->scsibus) {
			DPRINTF(ACDB_BOOTDEV, ("\t-- matched component %s to %s\n",
			    bp->name, dev->dv_xname));
			bootpath_store(1, bp + 1);
			return;
		}
	}

	if (strcmp(devname, "sd") == 0 || strcmp(devname, "cd") == 0) {
		/*
		 * A SCSI disk or cd; retrieve target/lun information
		 * from parent and match with current bootpath component.
		 * Note that we also have look back past the `scsibus'
		 * device to determine whether this target is on the
		 * correct controller in our boot path.
		 */
		struct scsi_attach_args *sa = aux;
		struct scsi_link *sl = sa->sa_sc_link;
		struct scsibus_softc *sbsc =
		    (struct scsibus_softc *)dev->dv_parent;
		u_int target = bp->val[0];
		u_int lun = bp->val[1];

		if (bp->val[0] & 0xffffffff00000000 && bp->val[0] != -1) {
			/* Fibre channel? */
			if (bp->val[0] == sl->port_wwn && lun == sl->lun) {
				nail_bootdev(dev, bp);
			}
			return;
		}

		/* Check the controller that this scsibus is on. */
		if ((bp-1)->dev != sbsc->sc_dev.dv_parent)
			return;

		/*
		 * Bounds check: we know the target and lun widths.
		 */
		if (target >= sl->adapter_buswidth ||
		    lun >= sl->luns) {
			printf("SCSI disk bootpath component not accepted: "
			       "target %u; lun %u\n", target, lun);
			return;
		}

		if (target == sl->target && lun == sl->lun) {
			nail_bootdev(dev, bp);
			return;
		}
	}

	if (strcmp("wd", devname) == 0) {
		/* IDE disks. */
		struct ata_atapi_attach *aa = aux;
		u_int channel, drive;

		if (strcmp(bp->name, "ata") == 0 &&
		    bp->val[0] == aa->aa_channel) {
			channel = bp->val[0]; bp++;
			drive = bp->val[0];
		} else {
			channel = bp->val[0] / 2;
			drive = bp->val[0] % 2;
		}

		if (channel == aa->aa_channel &&
		    drive == aa->aa_drv_data->drive) {
			nail_bootdev(dev, bp);
			return;
		}
	}
}

void
nail_bootdev(dev, bp)
	struct device *dev;
	struct bootpath *bp;
{

	if (bp->dev != NULL)
		panic("device_register: already got a boot device: %s",
			bp->dev->dv_xname);

	/*
	 * Mark this bootpath component by linking it to the matched
	 * device. We pick up the device pointer in cpu_rootconf().
	 */
	booted_device = bp->dev = dev;
	DPRINTF(ACDB_BOOTDEV, ("\t-- found bootdevice: %s\n",dev->dv_xname));

	/*
	 * Then clear the current bootpath component, so we don't spuriously
	 * match similar instances on other busses, e.g. a disk on
	 * another SCSI bus with the same target.
	 */
	bootpath_store(1, NULL);
}

struct nam2blk nam2blk[] = {
	{ "sd",		 7 },
	{ "rd",		 5 },
	{ "wd",		12 },
	{ "cd",		18 },
	{ "raid",	25 },
	{ NULL,		-1 }
};
