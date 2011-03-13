/*	$OpenBSD: promdev.c,v 1.13 2011/03/13 00:13:53 deraadt Exp $	*/
/*	$NetBSD: promdev.c,v 1.16 1995/11/14 15:04:01 pk Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Note: the `#ifndef BOOTXX' in here serve to queeze the code size
 * of the 1st-stage boot program.
 */
#include <sys/param.h>
#include <sys/reboot.h>
#include <machine/idprom.h>
#include <machine/oldmon.h>
#include <machine/ctlreg.h>

#include <lib/libsa/stand.h>

#include <sparc/stand/common/promdev.h>

/* u_long	_randseed = 1; */


int	obp_close(struct open_file *);
int	obp_strategy(void *, int, daddr32_t, size_t, void *, size_t *);
ssize_t	obp_xmit(struct promdata *, void *, size_t);
ssize_t	obp_recv(struct promdata *, void *, size_t);
int	prom0_close(struct open_file *);
int	prom0_strategy(void *, int, daddr32_t, size_t, void *, size_t *);
void	prom0_iclose(struct saioreq *);
int	prom0_iopen(struct promdata *);
ssize_t	prom0_xmit(struct promdata *, void *, size_t);
ssize_t	prom0_recv(struct promdata *, void *, size_t);

static char	*prom_mapin(u_long, int, int);

int	prom_findnode(int, const char *);
int	prom_findroot(void);
int	prom_firstchild(int);
int	getdevtype(int, char *);
int	prom_getprop(int, char *, void *, int);
int	prom_getproplen(int, char *);
char	*prom_getpropstring(int, char *);
int	prom_nextsibling(int);

static void	prom0_fake(void);

extern struct fs_ops file_system_nfs[];
extern struct fs_ops file_system_cd9660[];
extern struct fs_ops file_system_ufs[];

int
prom_open(struct open_file *f, ...)
{
	return 0;
}

int
prom_ioctl(struct open_file *f, u_long c, void *d)
{
	return EIO;
}

struct devsw devsw[] = {
	{ "prom0", prom0_strategy, prom_open, prom0_close, prom_ioctl },
	{ "prom", obp_strategy, prom_open, obp_close, prom_ioctl }
};

int	ndevs = (sizeof(devsw)/sizeof(devsw[0]));

char	*prom_bootdevice;
char	*prom_bootfile;
int	prom_boothow;

struct	promvec	*promvec;
static int	saveecho;

int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	int	error = 0, fd;
	struct	promdata *pd;

	pd = (struct promdata *)alloc(sizeof *pd);

	if (cputyp == CPU_SUN4) {
		error = prom0_iopen(pd);
#ifndef BOOTXX
		pd->xmit = prom0_xmit;
		pd->recv = prom0_recv;
#endif
	} else {
		fd = (promvec->pv_romvec_vers >= 2)
			? (*promvec->pv_v2devops.v2_open)(prom_bootdevice)
			: (*promvec->pv_v0devops.v0_open)(prom_bootdevice);
		if (fd == 0) {
			error = ENXIO;
		} else {
			pd->fd = fd;
#ifndef BOOTXX
			pd->xmit = obp_xmit;
			pd->recv = obp_recv;
#endif
		}
	}

	if (error) {
		printf("Can't open device `%s'\n", prom_bootdevice);
		return (error);
	}

#ifdef BOOTXX
	pd->devtype = DT_BLOCK;
#else /* BOOTXX */
	pd->devtype = getdevtype(fd, prom_bootdevice);
	/* Assume type BYTE is a raw device */
	if (pd->devtype != DT_BYTE)
		*file = (char *)fname;

	if (pd->devtype == DT_NET) {
		bcopy(file_system_nfs, file_system, sizeof(struct fs_ops));
		if ((error = net_open(pd)) != 0) {
			printf("Can't open network device `%s'\n",
				prom_bootdevice);
			return error;
		}
	} else {
		bcopy(file_system_ufs, file_system, sizeof(struct fs_ops));
		bcopy(&file_system_cd9660, file_system + 1, sizeof file_system[0]);
		nfsys = 2;
	}
#endif /* BOOTXX */

	f->f_dev = &devsw[cputyp == CPU_SUN4 ? 0 : 1];
	f->f_devdata = (void *)pd;
	return 0;
}

int
obp_strategy(devdata, flag, dblk, size, buf, rsize)
	void	*devdata;
	int	flag;
	daddr32_t	dblk;
	size_t	size;
	void	*buf;
	size_t	*rsize;
{
	int	error = 0;
	struct	promdata *pd = (struct promdata *)devdata;
	int	fd = pd->fd;

#ifdef DEBUG_PROM
	printf("promstrategy: size=%d dblk=%d\n", size, dblk);
#endif

	if (promvec->pv_romvec_vers >= 2) {
		if (pd->devtype == DT_BLOCK)
			(*promvec->pv_v2devops.v2_seek)(fd, 0, dbtob(dblk));

		*rsize = (*((flag == F_READ) ?
		    (u_int (*)(int, char *, size_t))promvec->pv_v2devops.v2_read :
		    (u_int (*)(int, char *, size_t))promvec->pv_v2devops.v2_write))
		    (fd, buf, size);
	} else {
		int n = (*((flag == F_READ) ?
		    (u_int (*)(int, int, daddr32_t, void *))promvec->pv_v0devops.v0_rbdev :
		    (u_int (*)(int, int, daddr32_t, void *))promvec->pv_v0devops.v0_wbdev))
		    (fd, btodb(size), dblk, buf);
		*rsize = dbtob(n);
	}

#ifdef DEBUG_PROM
	printf("rsize = %x\n", *rsize);
#endif
	return error;
}

/*
 * On old-monitor machines, things work differently.
 */
int
prom0_strategy(devdata, flag, dblk, size, buf, rsize)
	void	*devdata;
	int	flag;
	daddr32_t	dblk;
	size_t	size;
	void	*buf;
	size_t	*rsize;
{
	struct promdata	*pd = devdata;
	struct saioreq	*si;
	struct om_boottable *ops;
	char	*dmabuf;
	int	si_flag;
	size_t	xcnt;

	si = pd->si;
	ops = si->si_boottab;

#ifdef DEBUG_PROM
	printf("prom_strategy: size=%d dblk=%d\n", size, dblk);
#endif

	dmabuf = dvma_mapin(buf, size);

	si->si_bn = dblk;
	si->si_ma = dmabuf;
	si->si_cc = size;

	si_flag = (flag == F_READ) ? SAIO_F_READ : SAIO_F_WRITE;
	xcnt = (*ops->b_strategy)(si, si_flag);
	dvma_mapout(dmabuf, size);

#ifdef DEBUG_PROM
	printf("disk_strategy: xcnt = %x\n", xcnt);
#endif

	if (xcnt <= 0)
		return (EIO);

	*rsize = xcnt;
	return (0);
}

int
obp_close(f)
	struct open_file *f;
{
	struct promdata *pd = f->f_devdata;
	register int fd = pd->fd;

#ifndef BOOTXX
	if (pd->devtype == DT_NET)
		net_close(pd);
#endif
	if (promvec->pv_romvec_vers >= 2)
		(void)(*promvec->pv_v2devops.v2_close)(fd);
	else
		(void)(*promvec->pv_v0devops.v0_close)(fd);
	return 0;
}

int
prom0_close(f)
	struct open_file *f;
{
	struct promdata *pd = f->f_devdata;

#ifndef BOOTXX
	if (pd->devtype == DT_NET)
		net_close(pd);
#endif
	prom0_iclose(pd->si);
	pd->si = NULL;
	*romp->echo = saveecho; /* Hmm, probably must go somewhere else */
	return 0;
}

#ifndef BOOTXX
ssize_t
obp_xmit(pd, buf, len)
	struct	promdata *pd;
	void	*buf;
	size_t	len;
{
	return (promvec->pv_romvec_vers >= 2
		? (*promvec->pv_v2devops.v2_write)(pd->fd, buf, len)
		: (*promvec->pv_v0devops.v0_wnet)(pd->fd, len, buf));
}

ssize_t
obp_recv(pd, buf, len)
	struct	promdata *pd;
	void	*buf;
	size_t	len;
{
	int n;

	n = (promvec->pv_romvec_vers >= 2
		? (*promvec->pv_v2devops.v2_read)(pd->fd, buf, len)
		: (*promvec->pv_v0devops.v0_rnet)(pd->fd, len, buf));
	return (n == -2 ? 0 : n);
}

ssize_t
prom0_xmit(pd, buf, len)
	struct	promdata *pd;
	void	*buf;
	size_t	len;
{
	struct saioreq	*si;
	struct saif	*sif;
	char		*dmabuf;
	int		rv;

	si = pd->si;
	sif = si->si_sif;
	if (sif == NULL) {
		printf("xmit: not a network device\n");
		return (-1);
	}
	dmabuf = dvma_mapin(buf, len);
	rv = sif->sif_xmit(si->si_devdata, dmabuf, len);
	dvma_mapout(dmabuf, len);

	return (ssize_t)(rv ? -1 : len);
}

ssize_t
prom0_recv(pd, buf, len)
	struct	promdata *pd;
	void	*buf;
	size_t	len;
{
	struct saioreq	*si;
	struct saif	*sif;
	char		*dmabuf;
	int		rv;

	si = pd->si;
	sif = si->si_sif;
	dmabuf = dvma_mapin(buf, len);
	rv = sif->sif_poll(si->si_devdata, dmabuf);
	dvma_mapout(dmabuf, len);

	return (ssize_t)rv;
}

int
getchar()
{
	char c;
	register int n;

	if (promvec->pv_romvec_vers > 2)
		while ((n = (*promvec->pv_v2devops.v2_read)
			(*promvec->pv_v2bootargs.v2_fd0, (caddr_t)&c, 1)) != 1);
	else {
                /* SUN4 PROM: must turn off local echo */
                struct om_vector *oldpvec = (struct om_vector *)PROM_BASE;
                int saveecho = 0;

                if (CPU_ISSUN4) {
                        saveecho = *(oldpvec->echo);
                        *(oldpvec->echo) = 0;
                }
                c = (*promvec->pv_getchar)();
                if (CPU_ISSUN4)
                        *(oldpvec->echo) = saveecho;
	}

	if (c == '\r')
		c = '\n';
	return (c);
}

int
cngetc(void)
{
	return getchar();
}

int
peekchar(void)
{
	char c;
	register int n;

	if (promvec->pv_romvec_vers > 2) {
		n = (*promvec->pv_v2devops.v2_read)
			(*promvec->pv_v2bootargs.v2_fd0, (caddr_t)&c, 1);
		if (n < 0)
			return -1;
	} else
		c = (*promvec->pv_nbgetchar)();

	if (c == '\r')
		c = '\n';
	return (c);
}
#endif

static void
pv_putchar(int c)
{
	char c0 = c;

	if (promvec->pv_romvec_vers > 2)
		(*promvec->pv_v2devops.v2_write)
			(*promvec->pv_v2bootargs.v2_fd1, &c0, 1);
	else
		(*promvec->pv_putchar)(c);
}

void
putchar(c)
	int c;
{

	if (c == '\n')
		pv_putchar('\r');
	pv_putchar(c);
}

void
_rtt()
{
	promvec->pv_halt();
}

#ifndef BOOTXX
int hz = 1000;

time_t
getsecs(void)
{
	register int ticks = getticks();
	return ((time_t)(ticks / hz));
}

int
getticks(void)
{
	if (promvec->pv_romvec_vers >= 2) {
		char c;
		(void)(*promvec->pv_v2devops.v2_read)
			(*promvec->pv_v2bootargs.v2_fd0, (caddr_t)&c, 0);
	} else {
		(void)(*promvec->pv_nbgetchar)();
	}
	return *(promvec->pv_ticks);
}

struct idprom *
prom_getidprom()
{
	if (cputyp == CPU_SUN4) {
		static struct idprom sun4_idprom;
		u_char *src, *dst;
		int len, x;

		if (sun4_idprom.id_format == 0) {
			dst = (char *)&sun4_idprom;
			src = (char *)AC_IDPROM;
			len = sizeof(struct idprom);
			do {
				x = lduba(src++, ASI_CONTROL);
				*dst++ = x;
			} while (--len > 0);
		}

		return &sun4_idprom;
	} else
		return NULL;
}

void
prom_getether(int fd, u_char *ea)
{
	if (cputyp == CPU_SUN4) {
		struct idprom *idp = prom_getidprom();
		bcopy(idp->id_ether, ea, 6);
	} else if (promvec->pv_romvec_vers <= 2) {
		(void)(*promvec->pv_enaddr)(fd, (char *)ea);
	} else {
		char buf[64];
		snprintf(buf, sizeof buf, "%x mac-address drop swap 6 cmove", ea);
		promvec->pv_fortheval.v2_eval(buf);
	}
}

/*
 * A number of well-known devices on sun4s.
 */
static struct dtab {
	char	*name;
	int	type;
} dtab[] = {
	{ "sd",	DT_BLOCK },
	{ "st",	DT_BYTE },
	{ "xd",	DT_BLOCK },
	{ "xy",	DT_BLOCK },
	{ "fd",	DT_BLOCK },
	{ "le",	DT_NET },
	{ "ie",	DT_NET },
	{ NULL, 0 }
};

int
getdevtype(fd, name)
	int	fd;
	char	*name;
{
	if (promvec->pv_romvec_vers >= 2) {
		int node = (*promvec->pv_v2devops.v2_fd_phandle)(fd);
		char *cp = prom_getpropstring(node, "device_type");
		if (strcmp(cp, "block") == 0)
			return DT_BLOCK;
		else if (strcmp(cp, "network") == 0)
			return DT_NET;
		else if (strcmp(cp, "byte") == 0)
			return DT_BYTE;
	} else {
		struct dtab *dp;
		for (dp = dtab; dp->name; dp++) {
			if (name[0] == dp->name[0] &&
			    name[1] == dp->name[1])
				return dp->type;
		}
	}
	return 0;
}

/*
 * OpenPROM nodes & property routines (from <sparc/autoconf.c>).
 */

int
prom_findnode(int first, const char *name)
{
	int node;

	for (node = first; node != 0; node = prom_nextsibling(node))
		if (strcmp(prom_getpropstring(node, "name"), name) == 0)
			return (node);
	return (0);
}

int
prom_findroot()
{
	static int rootnode;
	int node;

	if ((node = rootnode) == 0 && (node = prom_nextsibling(0)) == 0)
		panic("no PROM root device");
	rootnode = node;
	return (node);
}

int
prom_firstchild(int node)
{
	return promvec->pv_nodeops->no_child(node);
}

int
prom_getprop(int node, char *name, void *buf, int bufsiz)
{
	register struct nodeops *no;
	register int len;

	no = promvec->pv_nodeops;
	len = no->no_proplen(node, name);
	if (len > bufsiz) {
		printf("node %x property %s length %d > %d\n",
		    node, name, len, bufsiz);
		return (0);
	}
	no->no_getprop(node, name, buf);
	return (len);
}

int
prom_getproplen(int node, char *name)
{
	return promvec->pv_nodeops->no_proplen(node, name);
}

/*
 * Return a string property.  There is a (small) limit on the length;
 * the string is fetched into a static buffer which is overwritten on
 * subsequent calls.
 */
char *
prom_getpropstring(int node, char *name)
{
	register int len;
	static char stringbuf[64];

	len = prom_getprop(node, name, (void *)stringbuf, sizeof stringbuf - 1);
	if (len == -1)
		len = 0;
	stringbuf[len] = '\0';	/* usually unnecessary */
	return (stringbuf);
}

int
prom_nextsibling(int node)
{
	return (promvec->pv_nodeops->no_nextnode(node));
}

void
prom_interpret(char *s)
{
	if (promvec->pv_romvec_vers < 2)
		promvec->pv_fortheval.v0_eval(strlen(s), s);
	else
		promvec->pv_fortheval.v2_eval(s);
}

int
prom_makememarr(struct memarr *ap, u_int xmax, int which)
{
	struct v0mlist *mp;
	int node, n;
	char *prop;

	if (which != MEMARR_AVAILPHYS && which != MEMARR_TOTALPHYS)
		panic("makememarr");

	if (CPU_ISSUN4) {
		struct om_vector *oldpvec = (struct om_vector *)PROM_BASE;
		if (ap != NULL && xmax != 0) {
			ap[0].addr_hi = 0;
			ap[0].addr_lo = 0;
			ap[0].len = which == MEMARR_AVAILPHYS ?
			    *oldpvec->memoryAvail : *oldpvec->memorySize;
		}
		return 1;
	}

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
			ap->addr_lo = (u_int)mp->addr;
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
		if ((node = prom_findnode(prom_firstchild(prom_findroot()),
		    "memory")) == 0)
			panic("makememarr: cannot find \"memory\" node");
		prop = which == MEMARR_AVAILPHYS ? "available" : "reg";
		n = prom_getproplen(node, prop) / sizeof(struct memarr);
		if (ap != NULL) {
			if (prom_getprop(node, prop, ap,
			    xmax * sizeof(struct memarr)) <= 0)
				panic("makememarr: cannot get property");
		}
		break;
	}

	if (n <= 0)
		panic("makememarr: no memory found");
	return (n);
}
#endif /* BOOTXX */

void
prom_init()
{
	char	*ap, *cp, *dp;
#ifndef BOOTXX
	int node;
#endif

	if (cputyp == CPU_SUN4) {
		prom0_fake();
		dvma_init();
	}

	if (promvec->pv_romvec_vers >= 2) {
		static char filestore[16];

		prom_bootdevice = *promvec->pv_v2bootargs.v2_bootpath;

#ifndef BOOTXX
		cp = *promvec->pv_v2bootargs.v2_bootargs;
		dp = prom_bootfile = filestore;
		while (*cp && *cp != '-')
			*dp++ = *cp++;
		while (dp > prom_bootfile && *--dp == ' ');
		*++dp = '\0';
		ap = cp;
#endif
	} else {
		static char bootstore[16];
		dp = prom_bootdevice = bootstore;
		cp = (*promvec->pv_v0bootargs)->ba_argv[0];
		while (*cp) {
			*dp++ = *cp;
			if (*cp++ == ')')
				break;
		}
		*dp = '\0';
#ifndef BOOTXX
		prom_bootfile = (*promvec->pv_v0bootargs)->ba_kernel;
		ap = (*promvec->pv_v0bootargs)->ba_argv[1];
#endif
	}

#ifndef BOOTXX
	if (ap != NULL && *ap == '-') {
		while (*ap) {
			switch (*ap++) {
			case 'a':
				prom_boothow |= RB_ASKNAME;
				break;
			case 'c':
				prom_boothow |= RB_CONFIG;
				break;
			case 'd':
				prom_boothow |= RB_KDB;
				debug = 1;
				break;
			case 's':
				prom_boothow |= RB_SINGLE;
				break;
			}
		}
	}
#endif

#ifndef BOOTXX
	/*
	 * Find out what type of machine we're running on.
	 *
	 * This process is actually started in srt0.S, which has discovered
	 * the minimal set of machine specific parameters for the 1st-level
	 * boot program (bootxx) to run. The CPU type is either CPU_SUN4 or
	 * CPU_SUN4C at this point; we need to figure out the exact cpu type
	 * and our page size.
	 */

	if (cputyp == CPU_SUN4) {
		pgshift = SUN4_PGSHIFT;
	} else {
		/*
		 * We are either SUN4C, SUN4D, SUN4E or SUN4M.
		 * Use the PROM `compatible' property to determine which.
		 * Absence of the `compatible' property means either sun4c
		 * or sun4e; these can be told apart by checking for the
		 * page size.
		 */

#ifdef BOOTXX
		char tmpstr[24];

		snprintf(tmpstr, sizeof tmpstr, "pagesize %x l!",
		    (u_long)&nbpg);
		prom_interpret(tmpstr);
		if (nbpg == 1 << SUN4_PGSHIFT)
			pgshift = SUN4_PGSHIFT;
		else
			pgshift = SUN4CM_PGSHIFT;
#else
		node = prom_findroot();
		cp = prom_getpropstring(node, "compatible");
		if (*cp == '\0' || strcmp(cp, "sun4c") == 0) {
			char tmpstr[24];

			snprintf(tmpstr, sizeof tmpstr, "pagesize %x l!",
			    (u_long)&nbpg);
			prom_interpret(tmpstr);
			if (nbpg == 1 << SUN4_PGSHIFT) {
				pgshift = SUN4_PGSHIFT;
				/* if netbooted, PROM won't output a cr */
				printf("\n");
			} else
				pgshift = SUN4CM_PGSHIFT;
			/* note that we don't bother telling 4e apart from 4c */
			cputyp = CPU_SUN4C;
		} else if (strcmp(cp, "sun4m") == 0) {
			cputyp = CPU_SUN4M;
			pgshift = SUN4CM_PGSHIFT;
#ifdef CPU_SUN4D
		} else if (strcmp(cp, "sun4d") == 0) {
			cputyp = CPU_SUN4D;
			pgshift = SUN4CM_PGSHIFT;
#endif
		} else
			panic("Unknown CPU type (compatible=`%s')", cp);
#endif	/* BOOTXX */
	}

	nbpg = 1 << pgshift;
	pgofset = nbpg - 1;
#endif
}

/*
 * Old monitor routines
 */

#include <machine/pte.h>

struct saioreq prom_si;
static int promdev_inuse;

int
prom0_iopen(pd)
	struct promdata	*pd;
{
	struct om_bootparam *bp;
	struct om_boottable *ops;
	struct devinfo *dip;
	struct saioreq *si;
	int	error;

	if (promdev_inuse)
		return(EMFILE);

	bp = *romp->bootParam;
	ops = bp->bootTable;
	dip = ops->b_devinfo;

#ifdef DEBUG_PROM
	printf("Boot device type: %s\n", ops->b_desc);
	printf("d_devbytes=%d\n", dip->d_devbytes);
	printf("d_dmabytes=%d\n", dip->d_dmabytes);
	printf("d_localbytes=%d\n", dip->d_localbytes);
	printf("d_stdcount=%d\n", dip->d_stdcount);
	printf("d_stdaddrs[%d]=%x\n", bp->ctlrNum, dip->d_stdaddrs[bp->ctlrNum]);
	printf("d_devtype=%d\n", dip->d_devtype);
	printf("d_maxiobytes=%d\n", dip->d_maxiobytes);
#endif

	si = &prom_si;
	bzero((caddr_t)si, sizeof(*si));
	si->si_boottab = ops;
	si->si_ctlr = bp->ctlrNum;
	si->si_unit = bp->unitNum;
	si->si_boff = bp->partNum;

	if (si->si_ctlr > dip->d_stdcount) {
		printf("Invalid controller number\n");
		return(ENXIO);
	}

	if (dip->d_devbytes) {
		si->si_devaddr = prom_mapin(dip->d_stdaddrs[si->si_ctlr],
			dip->d_devbytes, dip->d_devtype);
#ifdef	DEBUG_PROM
		printf("prom_iopen: devaddr=0x%x pte=0x%x\n",
			si->si_devaddr,
			getpte((u_long)si->si_devaddr & ~PGOFSET));
#endif
	}

	if (dip->d_dmabytes) {
		si->si_dmaaddr = dvma_alloc(dip->d_dmabytes);
#ifdef	DEBUG_PROM
		printf("prom_iopen: dmaaddr=0x%x\n", si->si_dmaaddr);
#endif
	}

	if (dip->d_localbytes) {
		si->si_devdata = alloc(dip->d_localbytes);
#ifdef	DEBUG_PROM
		printf("prom_iopen: devdata=0x%x\n", si->si_devdata);
#endif
	}

	/* OK, call the PROM device open routine. */
	error = (*ops->b_open)(si);
	if (error != 0) {
		printf("prom_iopen: \"%s\" error=%d\n",
			   ops->b_desc, error);
		return (ENXIO);
	}
#ifdef	DEBUG_PROM
	printf("prom_iopen: succeeded, error=%d\n", error);
#endif

	pd->si = si;
	promdev_inuse++;
	return (0);
}

void
prom0_iclose(si)
	struct saioreq *si;
{
	struct om_boottable *ops;
	struct devinfo *dip;

	if (promdev_inuse == 0)
		return;

	ops = si->si_boottab;
	dip = ops->b_devinfo;

	(*ops->b_close)(si);

	if (si->si_dmaaddr) {
		dvma_free(si->si_dmaaddr, dip->d_dmabytes);
		si->si_dmaaddr = NULL;
	}

	promdev_inuse = 0;
}

static struct mapinfo {
	int maptype;
	int pgtype;
	int base;
} prom_mapinfo[] = {
	{ MAP_MAINMEM,   PG_OBMEM, 0 },
	{ MAP_OBIO,      PG_OBIO,  0 },
	{ MAP_MBMEM,     PG_VME16, 0xFF000000 },
	{ MAP_MBIO,      PG_VME16, 0xFFFF0000 },
	{ MAP_VME16A16D, PG_VME16, 0xFFFF0000 },
	{ MAP_VME16A32D, PG_VME32, 0xFFFF0000 },
	{ MAP_VME24A16D, PG_VME16, 0xFF000000 },
	{ MAP_VME24A32D, PG_VME32, 0xFF000000 },
	{ MAP_VME32A16D, PG_VME16, 0 },
	{ MAP_VME32A32D, PG_VME32, 0 },
};
static prom_mapinfo_cnt = sizeof(prom_mapinfo) / sizeof(prom_mapinfo[0]);

/* The virtual address we will use for PROM device mappings. */
static u_long prom_devmap = MONSHORTSEG;

static char *
prom_mapin(physaddr, length, maptype)
	u_long physaddr;
	int length, maptype;
{
	int i, pa, pte, va;

	if (length > (4*NBPG))
		panic("prom_mapin: length=%d", length);

	for (i = 0; i < prom_mapinfo_cnt; i++)
		if (prom_mapinfo[i].maptype == maptype)
			goto found;
	panic("prom_mapin: invalid maptype %d", maptype);
found:

	pte = prom_mapinfo[i].pgtype;
	pte |= (PG_V|PG_W|PG_S|PG_NC);
	pa = prom_mapinfo[i].base;
	pa += physaddr;
	pte |= ((pa >> PGSHIFT) & PG_PFNUM);

	va = prom_devmap;
	do {
		setpte(va, pte);
		va += NBPG;
		pte += 1;
		length -= NBPG;
	} while (length > 0);
	return ((char *)(prom_devmap | (pa & PGOFSET)));
}

void
prom0_fake()
{
static	struct promvec promvecstore;

	promvec = &promvecstore;

	promvec->pv_stdin = romp->inSource;
	promvec->pv_stdout = romp->outSink;
	promvec->pv_putchar = romp->putChar;
	promvec->pv_putstr = romp->fbWriteStr;
	promvec->pv_nbgetchar = romp->mayGet;
	promvec->pv_getchar = romp->getChar;
	promvec->pv_romvec_vers = 0;            /* eek! */
	promvec->pv_reboot = romp->reBoot;
	promvec->pv_abort = romp->abortEntry;
	promvec->pv_setctxt = romp->setcxsegmap;
	promvec->pv_v0bootargs = (struct v0bootargs **)(romp->bootParam);
	promvec->pv_halt = romp->exitToMon;
	promvec->pv_ticks = romp->nmiClock;
	saveecho = *romp->echo;
	*romp->echo = 0;
}
