/*	$OpenBSD: rootfil.c,v 1.14 2002/09/17 02:37:20 hugh Exp $	*/
/*	$NetBSD: rootfil.c,v 1.14 1996/10/13 03:35:58 christos Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah Hdr: machdep.c 1.63 91/04/24
 *
 *	@(#)machdep.c	7.16 (Berkeley) 6/3/91
 */
 /* All bugs are subject to removal without further notice */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <dev/cons.h>

#include <machine/macros.h>
#include <machine/nexus.h>
#include <machine/sid.h>
#include <machine/disklabel.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/rpb.h>

#include "hp.h"
#include "ra.h"
#include "sd.h"
#include "asc.h"

void    setroot(void);
void    diskconf(void);
static  int getstr(char *, int);
struct  device *parsedisk(char *, int, int, dev_t *);
static  struct device *getdisk(char *, int, int, dev_t *);
int     findblkmajor(struct device *);
char    *findblkname(int);

struct  ngcconf {
        struct  cfdriver *ng_cf;
        dev_t   ng_root;
};

int	findblkmajor(struct device *);

struct device *bootdv = NULL;
int booted_partition;	/* defaults to 0 (aka 'a' partition */

extern dev_t rootdev, dumpdev;

#ifdef RAMDISK_HOOKS
static struct device fakerdrootdev = { DV_DISK, {}, NULL, 0, "rd0", NULL };
#endif

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
void
setroot()
{
	struct swdevt *swp;
	int  len, majdev, unit, part = 0;
	dev_t nrootdev, nswapdev, temp;
	extern int boothowto;
	struct device *dv;
	char buf[128];
#if defined(NFSCLIENT)
        extern char *nfsbootdevname;
#endif

	if (rpb.rpb_base != (void *)-1) {
#if DEBUG
		printf("booted from type %d unit %d csr 0x%lx adapter %lx slave %d\n",
		    rpb.devtyp, rpb.unit, rpb.csrphy, rpb.adpphy, rpb.slave);
#endif
		bootdev = MAKEBOOTDEV(rpb.devtyp, 0, 0, rpb.unit, 0);
	}

#ifdef RAMDISK_HOOKS
	bootdv = &fakerdrootdev;
#endif

	printf("booted from device: %s\n",
	    bootdv ? bootdv->dv_xname : "<unknown>");

	if (bootdv == NULL)
		boothowto |= RB_ASKNAME;

	if (boothowto & RB_ASKNAME) {
		for (;;) {
			printf("root device ");
			if (bootdv != NULL)
				printf("(default %s%c)",
				    bootdv->dv_xname,
				    bootdv->dv_class == DV_DISK
					? booted_partition + 'a' : ' ');
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
			dv = getdisk(buf, len, booted_partition, &nrootdev);
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
				    bootdv->dv_class == DV_DISK ? 'b' : ' ');
			printf(": ");
                        len = getstr(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				switch (bootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = MAKEDISKDEV(major(nrootdev),
					    DISKUNIT(nrootdev), 1);
					break;
				case DV_TAPE:
				case DV_TTY:
				case DV_DULL:
				case DV_CPU:
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
		majdev = findblkmajor(bootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on a disk.
			 * val[2] of the boot device is the partition number.
			 * Assume swap is on partition b.
			 */
			part = booted_partition;
			unit = bootdv->dv_unit;
			rootdev = MAKEDISKDEV(majdev, unit, part);
			nswapdev = dumpdev = MAKEDISKDEV(major(rootdev),
			    DISKUNIT(rootdev), 1);
		} else {
			/*
			 * Root and swap are on a net.
			 */
			nswapdev = dumpdev = NODEV;
		}
		swdevt[0].sw_dev = nswapdev;
		/* swdevt[1].sw_dev = NODEV; */

	} else {

		/*
		 * `root DEV swap DEV': honour rootdev/swdevt.
		 * rootdev/swdevt/mountroot already properly set.
		 */
		if (bootdv->dv_class == DV_DISK)
			printf("root on %s%c\n", bootdv->dv_xname,
			    part + 'a');
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		return;
	}

	switch (bootdv->dv_class) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = bootdv->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = dk_mountroot;
		majdev = major(rootdev);
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		printf("root on %s%c\n", bootdv->dv_xname, part + 'a');
		break;
	default:
		printf("can't figure root, hope your kernel is right\n");
		return;
	}

	/*
	 * Make the swap partition on the root drive the primary swap.
	 */
	temp = NODEV;
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    unit == DISKUNIT(swp->sw_dev)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev != NODEV) {
		/*
		 * If dumpdev was the same as the old primary swap device,
		 * move it to the new primary swap device.
		 */
		if (temp == dumpdev)
			dumpdev = swdevt[0].sw_dev;
	}
}

struct nam2blk {
        char *name;
        int maj;
} nam2blk[] = {
        { "ra",          9 },
        { "rx",         12 },
        { "rl",         14 },
        { "sd",         20 },
        { "rd",         23 },
        { "raid",       25 },
        { "cd",         61 },
};

int
findblkmajor(dv)
	struct device *dv;
{
	char *name = dv->dv_xname;
	int i;

	for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
		if (strncmp(name, nam2blk[i].name, strlen(nam2blk[i].name)) == 0)
			return (nam2blk[i].maj);
	return (-1);
}

char *
findblkname(maj)
        int maj;
{
        int i;

        for (i = 0; i < sizeof(nam2blk)/sizeof(nam2blk[0]); ++i)
                if (nam2blk[i].maj == maj)
                        return (nam2blk[i].name);
        return (NULL);
}


static struct device *
getdisk(str, len, defpart, devp)
        char *str;
        int len, defpart;
        dev_t *devp;
{
        struct device *dv;

        if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
                printf("use one of:");
#ifdef RAMDISK_HOOKS
                printf(" %s[a-p]", fakerdrootdev.dv_xname);
#endif
                for (dv = alldevs.tqh_first; dv != NULL;
                    dv = dv->dv_list.tqe_next) {        
                        if (dv->dv_class == DV_DISK)
                                printf(" %s[a-p]", dv->dv_xname);
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
        struct device *dv;
        char *cp, c;
        int majdev, unit, part;

        if (len == 0)
                return (NULL);
        cp = str + len - 1;
        c = *cp;
        if (c >= 'a' && (c - 'a') < MAXPARTITIONS) {
                part = c - 'a';
                *cp = '\0';
        } else
                part = defpart;

#ifdef RAMDISK_HOOKS
        if (strcmp(str, fakerdrootdev.dv_xname) == 0) {
                dv = &fakerdrootdev;
                goto gotdisk;
        }
#endif

        for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next) {
                if (dv->dv_class == DV_DISK &&
                    strcmp(str, dv->dv_xname) == 0) {
#ifdef RAMDISK_HOOKS
gotdisk:
#endif
                        majdev = findblkmajor(dv);
                        unit = dv->dv_unit;
                        if (majdev < 0)
                                panic("parsedisk");
                        *devp = MAKEDISKDEV(majdev, unit, part);
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

static int
getstr(char *buf, int size) {
	int len;
	cnpollc(1);
	len = getsn(buf, size);
	cnpollc(0);
	return (len);
}

/*
 * Configure swap space and related parameters.
 */
void
swapconf()
{
	struct swdevt *swp;
	u_int maj;
	int nblks;

	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {

		maj = major(swp->sw_dev);
		if (maj > nblkdev) /* paranoid? */
			break;

		if (bdevsw[maj].d_psize) {
			nblks = (*bdevsw[maj].d_psize)(swp->sw_dev);
			if (nblks > 0 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
}
