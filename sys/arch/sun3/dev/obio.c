/*	$NetBSD: obio.c,v 1.16 1995/02/13 22:23:57 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass and Gordon Ross.
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/mon.h>
#include <machine/isr.h>
#include <machine/obio.h>

static void obio_attach __P((struct device *, struct device *, void *));
static void obio_scan __P((struct device *, void *));

struct cfdriver obiocd = {
	NULL, "obio", always_match, obio_attach, DV_DULL,
	sizeof(struct device), 0 };

static void
obio_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	printf("\n");
	config_scan(obio_scan, self);
}

static void
obio_scan(parent, child)
	struct device *parent;
	void *child;
{
	bus_scan(parent, child, BUS_OBIO);
}

/*
 * Spacing of "interesting" OBIO mappings.  We will
 * record only those with an OBIO address that is a
 * multiple of SAVE_INCR and below SAVE_LAST.
 * The saved mappings are just one page each, which
 * is good enough for all the devices that use this.
 */
#define SAVE_SHIFT 17
#define SAVE_INCR (1<<SAVE_SHIFT)
#define SAVE_MASK (SAVE_INCR-1)
#define SAVE_SLOTS  16
#define SAVE_LAST (SAVE_SLOTS * SAVE_INCR)

/*
 * This is our record of "interesting" OBIO mappings that
 * the PROM has left in the virtual space reserved for it.
 * Each non-null array element holds the virtual address
 * of an OBIO mapping where the OBIO address mapped is:
 *     (array_index * SAVE_INCR)
 * and the length of the mapping is one page.
 */
static caddr_t prom_mappings[SAVE_SLOTS];

caddr_t obio_find_mapping(int pa, int size)
{
	if ((size <= NBPG) &&
		(pa < SAVE_LAST) &&
		((pa & SAVE_MASK) == 0))
	{
		return prom_mappings[pa >> SAVE_SHIFT];
	}
	return (caddr_t)0;
}

/*
 * This defines the permission bits to put in our PTEs.
 * Device space is never cached, and the PROM appears to
 * leave off the "no-cache" bit, so we can do the same.
 */
#define PGBITS (PG_VALID|PG_WRITE|PG_SYSTEM)

static void save_prom_mappings()
{
	vm_offset_t pa;
	caddr_t segva, pgva;
	int pte, sme, i;
	
	segva = (caddr_t)MONSTART;
	while (segva < (caddr_t)MONEND) {
		sme = get_segmap(segva);
		if (sme == SEGINV) {
			segva += NBSG;
			continue;			/* next segment */
		}
		/*
		 * We have a valid segmap entry, so examine the
		 * PTEs for all the pages in this segment.
		 */
		pgva = segva;	/* starting page */
		segva += NBSG;	/* ending page (next seg) */
		while (pgva < segva) {
			pte = get_pte(pgva);
			if ((pte & (PG_VALID | PG_TYPE)) ==
				(PG_VALID | PGT_OBIO))
			{
				/* Have a valid OBIO mapping. */
				pa = PG_PA(pte);
				/* Is it one we want to record? */
				if ((pa < SAVE_LAST) &&
					((pa & SAVE_MASK) == 0))
				{
					i = pa >> SAVE_SHIFT;
					if (prom_mappings[i] == NULL) {
						prom_mappings[i] = pgva;
#ifdef	DEBUG
						mon_printf("obio: found pa=0x%x\n", pa);
#endif
					}
				}
				/* Make sure it has the right permissions. */
				if ((pte & PGBITS) != PGBITS) {
#ifdef	DEBUG
					mon_printf("obio: fixing pte=0x%x\n", pte);
#endif
					pte |= PGBITS;
					set_pte(pgva, pte);
				}
			}
			pgva += NBPG;		/* next page */
		}
	}
}

/*
 * These are all the OBIO address that are required early in
 * the life of the kernel.  All are less than one page long.
 */
static vm_offset_t required_mappings[] = {
	/* Basically the first six OBIO devices. */
	OBIO_KEYBD_MS,
	OBIO_ZS,
	OBIO_EEPROM,
	OBIO_CLOCK,
	OBIO_MEMERR,
	OBIO_INTERREG,
	(vm_offset_t)-1,	/* end marker */
};

static void make_required_mappings()
{
	vm_offset_t pa, *rmp;
	int idx;
	
	rmp = required_mappings;
	while (*rmp != (vm_offset_t)-1) {
		if (!obio_find_mapping(*rmp, NBPG)) {
			/*
			 * XXX - Ack! Need to create one!
			 * I don't think this can happen, but if
			 * it does, we can allocate a PMEG in the
			 * "high segment" and add it there. -gwr
			 */
			mon_printf("obio: no mapping for 0x%x\n", *rmp);
			mon_panic("obio: Ancient PROM?\n");
		}
		rmp++;
	}
}


/*
 * this routine "configures" any internal OBIO devices which must be
 * accessible before the mainline OBIO autoconfiguration as part of
 * configure().
 */
void obio_init()
{
	save_prom_mappings();
	make_required_mappings();
}

caddr_t obio_alloc(obio_addr, obio_size)
	int obio_addr, obio_size;
{
	caddr_t cp;

	cp = obio_find_mapping((vm_offset_t)obio_addr, obio_size);
	if (cp) return (cp);

	cp = bus_mapin(BUS_OBIO, obio_addr, obio_size);
	return (cp);
}
