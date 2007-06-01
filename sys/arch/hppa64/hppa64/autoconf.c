/*	$OpenBSD: autoconf.c,v 1.9 2007/06/01 19:25:09 deraadt Exp $	*/

/*
 * Copyright (c) 1998-2005 Michael Shalayeff
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

#include "pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/malloc.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/reg.h>

#include <dev/cons.h>

#include <hppa/dev/cpudevs.h>

#if NPCI > 0
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#endif

void	dumpconf(void);

void (*cold_hook)(int); /* see below */

/* device we booted from */
struct device *bootdv;

/*
 * LED blinking thing
 */
#ifdef USELEDS
#include <sys/dkstat.h>
#include <sys/kernel.h>

struct timeout heartbeat_tmo;
void heartbeat(void *);
#endif

#include "cd.h"
#include "sd.h"
#include "st.h"
#if NCD > 0 || NSD > 0 || NST > 0
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#endif

/*
 * cpu_configure:
 * called at boot time, configure all devices on system
 */
void
cpu_configure(void)
{
	struct confargs ca;

	splhigh();
	bzero(&ca, sizeof(ca));
	if (config_rootfound("mainbus", &ca) == NULL)
		panic("no mainbus found");

	mtctl(0xffffffffffffffffULL, CR_EIEM);
	spl0();

	if (cold_hook)
		(*cold_hook)(HPPA_COLD_HOT);

#ifdef USELEDS
	timeout_set(&heartbeat_tmo, heartbeat, NULL);
	heartbeat(NULL);
#endif
	cold = 0;
}

void
diskconf(void)
{
	print_devpath("boot path", &PAGE0->mem_boot);
	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();
}

#ifdef USELEDS
/*
 * turn the heartbeat alive.
 * right thing would be to pass counter to each subsequent timeout
 * as an argument to heartbeat() incrementing every turn,
 * i.e. avoiding the static hbcnt, but doing timeout_set() on each
 * timeout_add() sounds ugly, guts of struct timeout looks ugly
 * to ponder in even more.
 */
void
heartbeat(v)
	void *v;
{
	static u_int hbcnt = 0, ocp_total, ocp_idle;
	int toggle, cp_mask, cp_total, cp_idle;

	timeout_add(&heartbeat_tmo, hz / 16);

	cp_idle = cp_time[CP_IDLE];
	cp_total = cp_time[CP_USER] + cp_time[CP_NICE] + cp_time[CP_SYS] +
	    cp_time[CP_INTR] + cp_time[CP_IDLE];
	if (cp_total == ocp_total)
		cp_total = ocp_total + 1;
	if (cp_idle == ocp_idle)
		cp_idle = ocp_idle + 1;
	cp_mask = 0xf0 >> (cp_idle - ocp_idle) * 4 / (cp_total - ocp_total);
	cp_mask &= 0xf0;
	ocp_total = cp_total;
	ocp_idle = cp_idle;
	/*
	 * do this:
	 *
	 *   |~| |~|
	 *  _| |_| |_,_,_,_
	 *   0 1 2 3 4 6 7
	 */
	toggle = 0;
	if (hbcnt++ < 8 && hbcnt & 1)
		toggle = PALED_HEARTBEAT;
	hbcnt &= 15;
	ledctl(cp_mask,
	    (~cp_mask & 0xf0) | PALED_NETRCV | PALED_NETSND | PALED_DISK,
	    toggle);
}
#endif

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf(void)
{
	extern int dumpsize;
	int nblks, dumpblks;	/* size of dump area */

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		return;
	dumpblks += ctod(physmem);

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1)))
		return;

	/* Put dump at end of partition */
	dumplo = nblks - dumpblks;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = physmem;
}


void
print_devpath(const char *label, struct pz_device *pz)
{
	int i;

	printf("%s: ", label);

	for (i = 0; i < 6; i++)
		if (pz->pz_bc[i] >= 0)
			printf("%d/", pz->pz_bc[i]);

	printf("%d.%x", pz->pz_mod, pz->pz_layers[0]);
	for (i = 1; i < 6 && pz->pz_layers[i]; i++)
		printf(".%x", pz->pz_layers[i]);

	printf(" class=%d flags=%b hpa=%p spa=%p io=%p\n", pz->pz_class,
	    pz->pz_flags, PZF_BITS, pz->pz_hpa, pz->pz_spa, pz->pz_iodc_io);
}

u_int32_t pdc_rt[16 / 4 * sizeof(struct pdc_pat_pci_rt)] PDC_ALIGNMENT;
struct pdc_sysmap_find pdc_find PDC_ALIGNMENT;
struct pdc_iodc_read pdc_iodc_read PDC_ALIGNMENT;
struct pdc_pat_cell_id pdc_pat_cell_id PDC_ALIGNMENT;
struct pdc_pat_cell_module pdc_pat_cell_module PDC_ALIGNMENT;
struct pdc_pat_io_num pdc_pat_io_num PDC_ALIGNMENT;
const char *pat_names[] = {
	"central",
	"cpu",
	"memory",
	"sba",
	"lba",
	"pbc",
	"cfc",
	"fabric"
};

void
pdc_scan(struct device *self, struct confargs *ca)
{
	struct device_path path;
	struct confargs nca;
	u_long	rv[16];
	int	i, err, mod = ca->ca_mod;

	if (pdc_call((iodcio_t)pdc, 0, PDC_PAT_CELL, PDC_PAT_CELL_GETID,
	    &pdc_pat_cell_id, 0))
		for (i = 0; !(err = pdc_call((iodcio_t)pdc, 0, PDC_SYSMAP,
		    PDC_SYSMAP_FIND, &pdc_find, &path, i)); i++) {
			if (autoconf_verbose)
				printf(">> hpa %x/%x dp %d/%d/%d/%d/%d/%d.%d\n",
				    pdc_find.hpa, pdc_find.size,
				    path.dp_bc[0], path.dp_bc[1], path.dp_bc[2],
				    path.dp_bc[3], path.dp_bc[4], path.dp_bc[5],
				    path.dp_mod);

			if (path.dp_bc[5] == mod) {
				nca = *ca;
				nca.ca_name = NULL;
				nca.ca_hpa = 0xffffffff00000000ULL |
				    (hppa_hpa_t)pdc_find.hpa;
				nca.ca_hpasz = pdc_find.size;
				nca.ca_mod = path.dp_mod;

				err = pdc_call((iodcio_t)pdc, 0, PDC_IODC,
				    PDC_IODC_READ, &pdc_iodc_read, nca.ca_hpa,
				    IODC_DATA, &nca.ca_type, sizeof(nca.ca_type));
				if (err < 0 || pdc_iodc_read.size < 8) {
					if (autoconf_verbose)
						printf(">> iodc_data error %d\n", err);
					bzero(&nca.ca_type, sizeof(nca.ca_type));
				}

				if (autoconf_verbose) {
					u_int *p = (u_int *)&nca.ca_type;
					printf(">> iodc_data 0x%08x 0x%08x\n",
					    p[0], p[1]);
				}

				config_found(self, &nca, mbprint);
			}
		}

	for (i = 0; !(err = pdc_call((iodcio_t)pdc, 0, PDC_PAT_CELL,
	    PDC_PAT_CELL_MODULE, rv, pdc_pat_cell_id.loc, i,
	    PDC_PAT_PAVIEW, &pdc_pat_cell_module, 0)); i++) {
		if (autoconf_verbose)
			printf(">> chpa %lx info %lx loc %lx "
			    "dp %d/%d/%d/%d/%d/%d.%d\n",
			    pdc_pat_cell_module.chpa, pdc_pat_cell_module.info,
			    pdc_pat_cell_module.loc,
			    pdc_pat_cell_module.dp.dp_bc[0],
			    pdc_pat_cell_module.dp.dp_bc[1],
			    pdc_pat_cell_module.dp.dp_bc[2],
			    pdc_pat_cell_module.dp.dp_bc[3],
			    pdc_pat_cell_module.dp.dp_bc[4],
			    pdc_pat_cell_module.dp.dp_bc[5],
			    pdc_pat_cell_module.dp.dp_mod);

		if (pdc_pat_cell_module.dp.dp_bc[5] == mod) {
			int t;

			t = PDC_PAT_CELL_MODTYPE(pdc_pat_cell_module.info);
			if (t >= sizeof(pat_names)/sizeof(pat_names[0]))
				continue;

			nca = *ca;
			nca.ca_name = pat_names[t];
			nca.ca_hpa = pdc_pat_cell_module.chpa &
			    ~(u_long)PAGE_MASK;
			nca.ca_hpasz =
			    PDC_PAT_CELL_MODSIZE(pdc_pat_cell_module.info);
			nca.ca_mod = pdc_pat_cell_module.dp.dp_mod;

			err = pdc_call((iodcio_t)pdc, 0, PDC_IODC,
			    PDC_IODC_READ, &pdc_iodc_read, nca.ca_hpa,
			    IODC_DATA, &nca.ca_type, sizeof(nca.ca_type));
			if (err < 0 || pdc_iodc_read.size < 8) {
				if (autoconf_verbose)
					printf(">> iodc_data error %d\n", err);
				bzero(&nca.ca_type, sizeof(nca.ca_type));
			}
			if (autoconf_verbose) {
				u_int *p = (u_int *)&nca.ca_type;
				printf(">> iodc_data 0x%08x 0x%08x\n",
				    p[0], p[1]);
			}

			config_found(self, &nca, mbprint);
		}
	}
}

struct pdc_pat_pci_rt *
pdc_getirt(int *pn)
{
	struct pdc_pat_pci_rt *rt;
	int i, num, err;
	long cell;

	cell = -1;
	if (!pdc_call((iodcio_t)pdc, 0, PDC_PAT_CELL, PDC_PAT_CELL_GETID,
	    &pdc_pat_cell_id, 0)) {
		cell = pdc_pat_cell_id.id;

		if ((err = pdc_call((iodcio_t)pdc, 0, PDC_PAT_IO,
		    PDC_PAT_IO_GET_PCI_RTSZ, &pdc_pat_io_num, cell))) {
			printf("irt size error %d\n", err);
			return (NULL);
		}
	} else if ((err = pdc_call((iodcio_t)pdc, 0, PDC_PCI_INDEX,
	    PDC_PCI_GET_INT_TBL_SZ, &pdc_pat_io_num, cpu_gethpa(0)))) {
		printf("irt size error %d\n", err);
		return (NULL);
	}

printf("num %ld ", pdc_pat_io_num.num);
	*pn = num = pdc_pat_io_num.num;
	if (num > sizeof(pdc_rt) / sizeof(*rt)) {
		printf("\nPCI IRT is too big %d\n", num);
		return (NULL);
	}

	if (!(rt = malloc(num * sizeof(*rt), M_DEVBUF, M_NOWAIT)))
		return (NULL);

	if (cell >= 0) {
		if ((err = pdc_call((iodcio_t)pdc, 0, PDC_PAT_IO,
		    PDC_PAT_IO_GET_PCI_RT, rt, cell))) {
			printf("irt fetch error %d\n", err);
			free(rt, M_DEVBUF);
			return (NULL);
		}
	} else if ((err = pdc_call((iodcio_t)pdc, 0, PDC_PCI_INDEX,
	    PDC_PCI_GET_INT_TBL, &pdc_pat_io_num, cpu_gethpa(0), pdc_rt))) {
		printf("irt fetch error %d\n", err);
		free(rt, M_DEVBUF);
		return (NULL);
	}
	bcopy(pdc_rt, rt, num * sizeof(*rt));

for (i = 0; i < num; i++)
printf("\n%d: ty 0x%02x it 0x%02x trig 0x%02x pin 0x%02x bus %d seg %d line %d addr 0x%llx",
i, rt[i].type, rt[i].itype, rt[i].trigger, rt[i].pin, rt[i].bus, rt[i].seg, rt[i].line, rt[i].addr);
	return rt;
}

const struct hppa_mod_info hppa_knownmods[] = {
#include <arch/hppa/dev/cpudevs_data.h>
};

const char *
hppa_mod_info(type, sv)
	int type, sv;
{
	const struct hppa_mod_info *mi;
	static char fakeid[32];

	for (mi = hppa_knownmods; mi->mi_type >= 0 &&
	    (mi->mi_type != type || mi->mi_sv != sv); mi++);

	if (mi->mi_type < 0) {
		snprintf(fakeid, sizeof fakeid, "type %x, sv %x", type, sv);
		return fakeid;
	} else
		return mi->mi_name;
}

void
device_register(struct device *dev, void *aux)
{
#if NPCI > 0
	extern struct cfdriver pci_cd;
#endif
	struct confargs *ca = aux;
	char *basename;
	static struct device *elder = NULL;

	if (bootdv != NULL)
		return;	/* We already have a winner */

#if NPCI > 0
	if (dev->dv_parent &&
	    dev->dv_parent->dv_cfdata->cf_driver == &pci_cd) {
		struct pci_attach_args *pa = aux;
		pcireg_t addr;
		int reg;

		for (reg = PCI_MAPREG_START; reg < PCI_MAPREG_END; reg += 4) {
			addr = pci_conf_read(pa->pa_pc, pa->pa_tag, reg);
			if (PCI_MAPREG_TYPE(addr) == PCI_MAPREG_TYPE_IO)
				addr = PCI_MAPREG_IO_ADDR(addr);
			else
				addr = PCI_MAPREG_MEM_ADDR(addr);

			if (addr == (pcireg_t)(u_long)PAGE0->mem_boot.pz_hpa) {
				elder = dev;
				break;
			}
		}
	} else
#endif
	if (ca->ca_hpa == (hppa_hpa_t)PAGE0->mem_boot.pz_hpa) {
		/*
		 * If hpa matches, the only thing we know is that the
		 * booted device is either this one or one of its children.
		 * And the children will not necessarily have the correct
		 * hpa value.
		 * Save this elder for now.
		 */
		elder = dev;
	} else if (elder == NULL) {
		return;	/* not the device we booted from */
	}

	/*
	 * Unfortunately, we can not match on pz_class vs dv_class on
	 * older snakes netbooting using the rbootd protocol.
	 * In this case, we'll end up with pz_class == PCL_RANDOM...
	 * Instead, trust the device class from what the kernel attached
	 * now...
	 */
	switch (dev->dv_class) {
	case DV_IFNET:
		/*
		 * Netboot is the top elder
		 */
		if (elder == dev) {
			bootdv = dev;
		}
		return;
	case DV_DISK:
		if ((PAGE0->mem_boot.pz_class & PCL_CLASS_MASK) != PCL_RANDOM)
			return;
		break;
	case DV_TAPE:
		if ((PAGE0->mem_boot.pz_class & PCL_CLASS_MASK) != PCL_SEQU)
			return;
		break;
	default:
		/* No idea what we were booted from, but better ask the user */
		return;
	}

	/*
	 * If control goes here, we are booted from a block device and we
	 * matched a block device.
	 */
	basename = dev->dv_cfdata->cf_driver->cd_name;

	/* TODO wd detect */

	/*
	 * We only grok SCSI boot currently. Match on proper device hierarchy,
	 * name and unit/lun values.
	 */
#if NCD > 0 || NSD > 0 || NST > 0
	if (strcmp(basename, "sd") == 0 || strcmp(basename, "cd") == 0 ||
	    strcmp(basename, "st") == 0) {
		struct scsi_attach_args *sa = aux;
		struct scsi_link *sl = sa->sa_sc_link;

		/*
		 * sd/st/cd is attached to scsibus which is attached to
		 * the controller. Hence the grandparent here should be
		 * the elder.
		 */
		if (dev->dv_parent == NULL ||
		    dev->dv_parent->dv_parent != elder) {
			return;
		}

		/*
		 * And now check for proper target and lun values
		 */
		if (sl->target == PAGE0->mem_boot.pz_layers[0] &&
		    sl->lun == PAGE0->mem_boot.pz_layers[1]) {
			bootdv = dev;
		}
	}
#endif
}

struct nam2blk nam2blk[] = {
	{ "rd",		3 },
	{ "sd",		4 },
	{ "st",		5 },
	{ "cd",		6 },
#if 0
	{ "wd",		? },
	{ "fd",		7 },
#endif
	{ NULL,		-1 }
};
