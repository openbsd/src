/*	$NetBSD: swapgeneric.c,v 1.14 1995/04/26 23:30:08 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Generic configuration (one kernel fits all 8-)
 * Some ideas taken from i386 port but no code.
 *
 * Allow root/swap on any of: le,sd
 * Eventually, allow: ie,st,xd,xy
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <machine/mon.h>
  
#ifdef	NFSCLIENT
extern char	*nfsbootdevname;	/* nfs_boot.c */
#else	/* NFSCLIENT */

int (*mountroot) __P((void)) = NULL;

dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

struct	swdevt swdevt[] = {
	{ NODEV,	0,	0 },
	{ NODEV,	0,	0 },
};

#define NAMESZ 16
char boot_ifname[NAMESZ];

/*
 * Functions to convert PROM ctlr/unit into our unit numbers
 */
static int net_mkunit(ctlr, unit)
	int ctlr, unit;
{
	/* XXX - Not sure which is set. */
	return (ctlr + unit);
}

static int sd_mkunit(ctlr, unit)
	int ctlr, unit;
{
	int target, lun;

	/* This only supports LUNs 0, 1 */
	target = unit >> 3;
	lun = unit & 1;
	return (target * 2 + lun);
}

static int xx_mkunit(ctlr, unit)
	int ctlr, unit;
{
	return (ctlr * 2 + unit);
}

/*
 * Devices which MIGHT be available.
 * If gc_root is NODEV, use NFS root.
 */
static struct genconf {
	char gc_name[4];
	int  gc_major;
	int  (*gc_mkunit)();
} genconf[] = {
	{ "ie", -1, net_mkunit },
	{ "le", -1, net_mkunit },
	{ "sd",	7,  sd_mkunit },
	{ "xy",	3,  xx_mkunit },
	{ "xd",	10, xx_mkunit },
	{ 0 },
};

static struct genconf *
gc_lookup(name)
	char *name;
{
	struct genconf *gc;

	gc = genconf;
	while (gc->gc_major) {
		if ((gc->gc_name[0] == name[0]) &&
			(gc->gc_name[1] == name[1]))
			return gc;
		gc++;
	}
	return NULL;
}

static void gc_print_all()
{
	struct genconf *gc;

	gc = genconf;
	for (;;) {
		printf("%s", gc->gc_name);
		gc++;
		if (gc->gc_major == 0)
			break;
		printf(", ");
	}
	printf("\n");
}
	

struct devspec {
	int  major;
	int  unit;
	int  part;
	char name[4];
};

/*
 * Set devspec from a string like: "sd0a"
 * Return length of recognized part.
 */
static int ds_parse(ds, str)
	struct devspec *ds;
	char *str;
{
	struct genconf *gc;
	int unit, part;
	char *p;

	bzero((caddr_t)ds, sizeof(*ds));
	while (*str == ' ')
		str++;
	p = str;

	gc = gc_lookup(p);
	if (!gc) return 0;

	/* Major number from the genconf table. */
	ds->major = gc->gc_major;

	/* Device name (always two letters on Suns) */
	ds->name[0] = *p++;
	ds->name[1] = *p++;

	/* Unit number */
	unit = 0;
	while ('0' <= *p && *p <= '9') {
		unit *= 10;
		unit += (*p++ - '0');
	}
	ds->unit = unit & 0x1F;

	/* Partition letter */
	part = 0;
	if ('a' <= *p && *p <= 'h')
		part = *p++ - 'a';
	ds->part = part;

	return (p - str);
}

/*
 * Format a devspec into a string like: "sd0a"
 * Returns length of string.
 */
static int ds_tostr(ds, str)
	struct devspec *ds;
	char *str;
{
	struct genconf *gc;
	int unit, part;
	char *p;

	p = str;

	/* Device name (always two letters on Suns) */
	*p++ = ds->name[0];
	*p++ = ds->name[1];

	/* Unit number */
	unit = ds->unit & 0x1f;
	if (unit >= 10) {
		*p++ = '0' + (unit / 10);
		unit = unit % 10;
	}
	*p++ = '0' + unit;

	/* Partition letter (only for disks). */
	if (ds->major >= 0) {
		part = ds->part & 7;
		*p++ = 'a' + part;
	}

	*p = '\0';
	return (p - str);
}

/*
 * Set the devspec to the device we booted from.
 * (Just converts PROM boot parameters.)
 */
static void ds_from_boot(ds)
	struct devspec *ds;
{
	MachMonBootParam *bpp;
	struct genconf *gc;

	bpp = *romp->bootParam;

	bzero((caddr_t)ds, sizeof(*ds));

	/* Device name (always two letters) */
	ds->name[0] = bpp->devName[0];
	ds->name[1] = bpp->devName[1];

	/* Is this device known? */
	gc = gc_lookup(ds->name);
	if (gc == NULL) {
		/* Boot device not in genconf, so ask. */
		boothowto |= RB_ASKNAME;
		return;
	}

	/* Compute our equivalents of the prom info. */
	ds->major = gc->gc_major;
	ds->unit = gc->gc_mkunit(bpp->ctlrNum, bpp->unitNum);
	ds->part = bpp->partNum;
}

/*
 * Fill in the devspec by asking the operator.
 * The ds passed may hold a default value.
 */
static void ds_query(ds, what)
	struct devspec *ds;
	char *what;
{
	struct genconf *gc;
	char *p;
	int len, minor;
	char buf[64];

	for (;;) {
		len = ds_tostr(ds, buf);
		printf("%s device? [%s] ", what, buf);

		gets(buf);
		if (buf[0] == '\0')
			return;

		len = ds_parse(ds, buf);
		if (len > 2)
			break;

		printf("Invalid name.  Use one of: ");
		gc_print_all();
	}
}

static dev_t ds_todev(ds)
	struct devspec *ds;
{
	int minor;
	if (ds->major < 0)
		return NODEV;
	minor = (ds->unit << 3) + (ds->part & 7);
	return (makedev(ds->major, minor));
}

/*
 * Choose the root and swap device, either by asking,
 * (if RB_ASKNAME) or from the PROM boot parameters.
 */
swapgeneric()
{
	struct genconf *gc;
	dev_t root, swap, dump;
	int minor;
	struct devspec ds;
	char buf[NAMESZ];

	/*
	 * Choose the root device.
	 * Default is boot device.
	 */
	ds_from_boot(&ds);
	if (boothowto & RB_ASKNAME)
		ds_query(&ds, "root");
	else {
		ds_tostr(&ds, buf);
		printf("root on %s\n", buf);
	}
	rootdev = ds_todev(&ds);

	/*
	 * Choose the root fstype.
	 * XXX - Hard coded for now.
	 */
#ifdef NFSCLIENT
	if (rootdev == NODEV) {
		/* Set boot interface name for nfs_mountroot. */
		nfsbootdevname = boot_ifname;
		ds_tostr(&ds, boot_ifname);
		mountroot = nfs_mountroot;
	} else
#endif /* NFSCLIENT */
	{
		/* XXX - Should ask for the root fstype here. -gwr */
		mountroot = dk_mountroot;
	}

	/*
	 * Choose the swap device.  (Default from root)
	 */
	ds.part = 1;
	if (boothowto & RB_ASKNAME)
		ds_query(&ds, "swap");
	else {
		ds_tostr(&ds, buf);
		printf("swap on %s\n", buf);
	}
	swdevt[0].sw_dev = ds_todev(&ds);

	/*
	 * Choose the dump device.  (Default from swap)
	 */
	if (boothowto & RB_ASKNAME)
		ds_query(&ds, "dump");
	else {
		ds_tostr(&ds, buf);
		printf("dump on %s\n", buf);
	}
	dumpdev = ds_todev(&ds);
}

/* XXX - Isn't this in some common file? */
gets(cp)
	char *cp;
{
	register char *lp;
	register c;

 top:
	lp = cp;
	for (;;) {
		c = cngetc();
		switch (c) {

		case '\n':
		case '\r':
			cnputc('\n');
			*lp++ = '\0';
			return;

		case '\b':
		case '\177':
			if (lp > cp) {
				lp--;
				printf("\b \b");
			}
			continue;

		case ('U'&037):
			cnputc('\n');
			goto top;

		default:
			if (c < ' ') {
				cnputc('^');
				*lp++ = '^';
				c |= 0100;
			}
			cnputc(c);
			*lp++ = c;
			break;
		}
	}
}
