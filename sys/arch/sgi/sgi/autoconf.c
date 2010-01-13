/*	$OpenBSD: autoconf.c,v 1.30 2010/01/13 22:57:29 miod Exp $	*/
/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2003-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * Copyright (c) 1996 Per Fogelstrom
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 */

/*
 * Setup the system to run on the current machine.
 *
 * cpu_configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/memconf.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <uvm/uvm_extern.h>

#include <sgi/sgi/ip30.h>
#include <sgi/xbow/xbow.h>
#include <dev/pci/pcivar.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

extern void dumpconf(void);

static u_long atoi(const char *, int, const char **);

void	bootpath_convert(void);
const char *bootpath_get(int *);
void	bootpath_init(void);
void	bootpath_next(void);

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold = 1;			/* if 1, still working on cold-start */
struct device *bootdv = NULL;
int16_t	currentnasid = 0;

char	osloadpartition[256];

/*
 *  Configure all devices found that we know about.
 *  This is done at boot time.
 */
void
cpu_configure(void)
{
	(void)splhigh();	/* Set mask to what we intend. */

	softintr_init();

	if (config_rootfound("mainbus", "mainbus") == 0) {
		panic("no mainbus found");
	}

	splinit();		/* Initialized, fire up interrupt system */
	cold = 0;
}

void
diskconf(void)
{
	if (bootdv == NULL)
		printf("boot device: '%s' unrecognized.\n", osloadpartition);
	else
		printf("boot device: %s\n", bootdv->dv_xname);

	setroot(bootdv, 0, RB_USERREQ);
	dumpconf();
}

/*
 * Register a memory region.
 */
int
memrange_register(uint64_t startpfn, uint64_t endpfn, uint64_t bmask,
    unsigned int freelist)
{
	struct phys_mem_desc *cur, *m = NULL;
	int i;

#ifdef DEBUG
{
	extern int console_ok;

	if (console_ok)
		printf("%s: memory from %p to %p\n",
		    __func__, ptoa(startpfn), ptoa(endpfn));
	else
		bios_printf("%s: memory from %p to %p\n",
		     __func__, ptoa(startpfn), ptoa(endpfn));
}
#endif
	physmem += endpfn - startpfn;

#ifdef TGT_OCTANE
	/*
	 * On Octane, the second 16KB page is reserved for the NMI handler.
	 */
	if (sys_config.system_type == SGI_OCTANE &&
	    startpfn < atop(IP30_MEMORY_BASE) + 2) {
		startpfn = atop(IP30_MEMORY_BASE) + 2;
		if (startpfn >= endpfn)
			return 0;
	}
#endif
	/*
	 * Prevent use of memory above 16GB physical, until pmap can support
	 * this.
	 */
	if (startpfn >= atop(16UL * 1024 * 1024 * 1024))
		return 0;
	if (endpfn >= atop(16UL * 1024 * 1024 * 1024))
		endpfn = atop(16UL * 1024 * 1024 * 1024);
	
	for (i = 0, cur = mem_layout; i < MAXMEMSEGS; i++, cur++) {
		if (cur->mem_last_page == 0) {
			if (m == NULL)
				m = cur;	/* first free segment */
			continue;
		}
		/* merge contiguous areas if on the same freelist */
		if (cur->mem_freelist == freelist) {
			if (cur->mem_first_page == endpfn &&
			    ((cur->mem_last_page ^ startpfn) & bmask) == 0) {
				cur->mem_first_page = startpfn;
				return 0;
			}
			if (cur->mem_last_page == startpfn &&
			    ((cur->mem_first_page ^ endpfn) & bmask) == 0) {
				cur->mem_last_page = endpfn;
				return 0;
			}
		}
	}

	if (m == NULL)
		return ENOMEM;
	
	m->mem_first_page = startpfn;
	m->mem_last_page = endpfn;
	m->mem_freelist = freelist;
	return 0;
}

static char bootpath_store[sizeof osloadpartition];
static char *bootpath_curpos;
static char *bootpath_lastpos;
static int bootpath_lastunit;
#ifdef TGT_ORIGIN
static int dksc_ctrl, dksc_mode;
#endif

/*
 * Initialize bootpath analysis.
 */
void
bootpath_init()
{
	strlcpy(bootpath_store, osloadpartition, sizeof bootpath_store);
	bootpath_curpos = bootpath_store;

#ifdef TGT_ORIGIN
	/*
	 * If this is the first time we're ever invoked,
	 * check for a dksc() syntax and rewrite it as
	 * something more friendly to us.
	 */
	if (strncmp(bootpath_store, "dksc(", 5) == 0)
		bootpath_convert();
#endif
}

#ifdef TGT_ORIGIN
/*
 * Convert a `dksc()' bootpath into an ARC-friendly bootpath.
 */
void
bootpath_convert()
{
	int val[3], idx;
	char *c;

	val[0] = val[1] = val[2] = 0;
	idx = 0;

	for (c = bootpath_store + 5; *c != '\0'; c++) {
		if (*c == ')')
			break;
		else if (*c == ',') {
			if (++idx == 3)
				break;
		} else if (*c >= '0' && *c <= '9')
			val[idx] = 10 * val[idx] + (*c - '0');
	}

	/*
	 * We can not convert the dksc() bootpath to an exact ARCS bootpath
	 * without knowing our device tree already.  This is because
	 * the controller number is not an absolute locator, but rather an
	 * occurence number.
	 *
	 * So we convert to an incomplete ARCS bootpath and have explicit
	 * dksc handling in device_register().  This relies on our device
	 * probe order matching ARCS.
	 */

	dksc_ctrl = val[0];
	dksc_mode = 1;
	snprintf(bootpath_store, sizeof bootpath_store,
	    "scsi(%d)disk(%d)rdisk(0)partition(%d)",
	    val[0], val[1], val[2]);
#ifdef DEBUG
	printf("%s: converting %s to %s\n",
	    __func__, osloadpartition, bootpath_store);
#endif
}
#endif

/*
 * Extract a component of the boot path, and return its name and unit
 * value.
 */
const char *
bootpath_get(int *u)
{
	char *c;
	int unit;

	/*
	 * If we don't have a value in cache, compute it.
	 */
	if (bootpath_lastpos == NULL) {
		if (bootpath_curpos == NULL)
			bootpath_init();

		unit = 0;
		c = strchr(bootpath_curpos, '(');
		if (c != NULL) {
			for (*c++ = '\0'; *c >= '0' && *c <= '9'; c++)
				unit = 10 * unit + (*c - '0');
			while (*c != ')' && *c != '\0')
				c++;
			if (*c == ')')
				c++;
		} else {
			c = bootpath_curpos + strlen(bootpath_curpos);
		}

		bootpath_lastpos = bootpath_curpos;
		bootpath_lastunit = unit;
		bootpath_curpos = c;
#ifdef DEBUG
		printf("%s: new component %s unit %d remainder %s\n", __func__,
		    bootpath_lastpos, bootpath_lastunit, bootpath_curpos);
#endif
	}

	*u = bootpath_lastunit;
	return bootpath_lastpos;
}

/*
 * Consume the current component of the bootpath, and switch to the next.
 */
void
bootpath_next()
{
	/* force bootpath_get to go forward */
	bootpath_lastpos = NULL;
#ifdef DEBUG
	printf("%s\n", __func__);
#endif
}

void
device_register(struct device *dev, void *aux)
{
	static struct device *lastparent = NULL;
	static struct device *pciparent = NULL;
	static int component_pos = 0;

	struct device *parent = dev->dv_parent;
	struct cfdata *cf = dev->dv_cfdata;
	struct cfdriver *cd = cf->cf_driver;

	const char *component;
	int unit;

	if (parent == NULL)
		return;		/* one of the @root devices */

	if (bootdv != NULL)
		return;

	component = bootpath_get(&unit);
	if (*component == '\0')
		return;		/* exhausted path */

	/*
	 * The matching rules are as follows:
	 * xio() matches xbow.
	 * pci() matches any pci controller (macepcibr, xbridge), with the
	 *   unit number being ignored on O2 and the widget number of the
	 *   controller elsewhere.
	 *   XXX I have no idea how this works when PIC devices are involved
	 *   XXX since they provide two distinct PCI buses...
	 *   XXX ...and our device numbering is off by one in that case.
	 * scsi() matches any pci scsi controller, with the unit number
	 *   being the pci device number (minus one on the O2, grr),
	 *   or the scsibus number in dksc mode.
	 * disk() and cdrom() match sd and cd, respectively, with the
	 *   unit number being the target number.
	 *
	 * When a disk is found, we stop the parsing; rdisk() and
	 * partition() components are ignored.
	 */

	if (strcmp(component, "xio") == 0) {
		struct mainbus_attach_args *maa = aux;

		if (strcmp(cd->cd_name, "xbow") == 0 && unit == maa->maa_nasid)
			goto found_advance;
	}

	if (strcmp(component, "pci") == 0) {
		/*
		 * We'll work in two steps. The controller itself will be
		 * recognized with its parent device and attachment
		 * arguments (if necessary).
		 *
		 * Then we'll only advance the bootpath when matching the
		 * pci device.
		 */
		if (strcmp(cd->cd_name, "pci") == 0 &&
		    parent == lastparent) {
			pciparent = dev;
			goto found_advance;
		}

		if (strcmp(cd->cd_name, "macepcibr") == 0)
			goto found;
		if (strcmp(cd->cd_name, "xbridge") == 0 &&
		    parent == lastparent) {
			struct xbow_attach_args *xaa = aux;

			if (unit == xaa->xaa_widget)
				goto found;
		}
		if (strcmp(cd->cd_name, "xbpci") == 0 &&
		    parent == lastparent) {
			if (1)	/* how to match the exact bus number? */
				goto found;
		}
	}

	if (strcmp(component, "scsi") == 0) {
		/*
		 * We'll work in two steps. The controller itself will be
		 * recognized with its parent device and pci_attach_args
		 * need to match the scsi() unit number.
		 *
		 * Then we'll only advance the bootpath when matching the
		 * scsibus device.
		 *
		 * With a dksc bootpath, things are a little different:
		 * we need to count scsi controllers, until we find ours.
		 */

#ifdef TGT_ORIGIN
		if (dksc_mode) {
			if (strcmp(cd->cd_name, "scsibus") == 0 &&
			    dev->dv_unit == dksc_ctrl)
				goto found_advance;

			return;
		}
#endif

		if (strcmp(cd->cd_name, "scsibus") == 0) {
			if (parent == lastparent)
				goto found_advance;

#ifdef TGT_O2
			/*
			 * On O2, the pci(0) component may be omitted from
			 * the bootpath, in which case we fake the missing
			 * pci(0) component.
			 */
			if (sys_config.system_type == SGI_O2 &&
			    component_pos == 0) {
				if (parent->dv_parent != NULL &&
				    strcmp(parent->dv_parent->dv_cfdata->cf_driver->cd_name,
				      "pci") == 0) {
					pciparent = parent->dv_parent;
					goto found_advance;
				}
			}
#endif
		}

		if (parent == lastparent) {
			if (parent == pciparent) {
				struct pci_attach_args *paa = aux;

				if (unit == paa->pa_device -
				    (sys_config.system_type == SGI_O2 ? 1 : 0))
					goto found;
			}
			/*
			 * in case scsi() can follow something else then
			 * pci(), write code to handle this here...
			 */
		}
	}

	if ((strcmp(component, "disk") == 0 &&
	     strcmp(cd->cd_name, "sd") == 0) ||
	    (strcmp(component, "cdrom") == 0 &&
	     strcmp(cd->cd_name, "cd") == 0)) {
		if (parent == lastparent) {
			struct scsi_attach_args *saa = aux;

			if (unit == saa->sa_sc_link->target) {
				/*
				 * We found our boot device.
				 * Now get the partition number.
				 */
				bootdv = dev;
#ifdef DEBUG
				printf("%s: boot device is %s\n",
				    __func__, dev->dv_xname);
#endif
				return;
			}
		}
	}

	return;

found_advance:
	bootpath_next();
	component_pos++;
found:
	lastparent = dev;
}

struct nam2blk nam2blk[] = {
	{ "sd",		0 },
	{ "wd",		4 },
	{ "rd",		8 },
	{ "vnd",	2 },
	{ NULL,		-1 }
};

/*
 * Convert "xx:xx:xx:xx:xx:xx" string to Ethernet hardware address.
 */
void
enaddr_aton(const char *s, u_int8_t *a)
{
	int i;

	if (s != NULL) {
		for (i = 0; i < 6; i++) {
			a[i] = atoi(s, 16, &s);
			if (*s == ':')
				s++;
		}
	}
}

/*
 * Get a numeric environment variable
 */
u_long
bios_getenvint(const char *name)
{
	const char *envvar;
	u_long value;

	envvar = Bios_GetEnvironmentVariable(name);
	if (envvar != NULL) {
		value = atoi(envvar, 10, &envvar);
		if (*envvar != '\0')
			value = 0;
	} else
		value = 0;

	return value;
}

/*
 * Convert an ASCII string into an integer.
 */
static u_long
atoi(const char *s, int b, const char **o)
{
	int c;
	unsigned base = b, d;
	int neg = 0;
	u_long val = 0;

	if (s == NULL || *s == 0) {
		if (o != NULL)
			*o = s;
		return 0;
	}

	/* Skip spaces if any. */
	do {
		c = *s++;
	} while (c == ' ' || c == '\t');

	/* Parse sign, allow more than one (compat). */
	while (c == '-') {
		neg = !neg;
		c = *s++;
	}

	/* Parse base specification, if any. */
	if (base == 0 && c == '0') {
		c = *s++;
		switch (c) {
		case 'X':
		case 'x':
			base = 16;
			c = *s++;
			break;
		case 'B':
		case 'b':
			base = 2;
			c = *s++;
			break;
		default:
			base = 8;
		}
	}

	/* Parse number proper. */
	for (;;) {
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'z')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'Z')
			d = c - 'A' + 10;
		else
			break;
		if (d >= base)
			break;
		val *= base;
		val += d;
		c = *s++;
	}
	if (neg)
		val = -val;
	if (o != NULL)
		*o = s - 1;
	return val;
}
