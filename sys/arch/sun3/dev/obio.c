/*	$OpenBSD: obio.c,v 1.7 1999/01/11 05:12:02 millert Exp $	*/
/*	$NetBSD: obio.c,v 1.23 1996/11/20 18:56:56 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/control.h>
#include <machine/pte.h>
#include <machine/mon.h>
#include <machine/obio.h>

static int  obio_match __P((struct device *, void *, void *));
static void obio_attach __P((struct device *, struct device *, void *));
static int  obio_print __P((void *, const char *parentname));
static int  obio_submatch __P((struct device *, void *, void *));

static void save_prom_mappings __P((void));
static void make_required_mappings __P((void));

struct cfattach obio_ca = {
	sizeof(struct device), obio_match, obio_attach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};

static int
obio_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;

	if (ca->ca_bustype != BUS_OBIO)
		return (0);
	return(1);
}

#define	OBIO_INCR	0x020000
#define OBIO_END	0x200000

static void
obio_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux;
	int	addr;

	printf("\n");

	/* Configure these in order of address. */
	for (addr = 0; addr < OBIO_END; addr += OBIO_INCR) {

		/* We know ca_bustype == BUS_OBIO */
		ca->ca_paddr = addr;
		ca->ca_intpri = -1;
		ca->ca_intvec = -1;

		(void) config_found_sm(self, ca, obio_print, obio_submatch);
	}
}

/*
 * Print out the confargs.  The (parent) name is non-NULL
 * when there was no match found by config_found().
 */
static int
obio_print(args, name)
	void *args;
	const char *name;
{
	struct confargs *ca = args;

	/* Be quiet about empty OBIO locations. */
	if (name)
		return(QUIET);

	if (ca->ca_paddr != -1)
		printf(" addr 0x%x", ca->ca_paddr);
	if (ca->ca_intpri != -1)
		printf(" level %d", ca->ca_intpri);

	return(UNCONF);
}

int
obio_submatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	cfmatch_t submatch;

	/*
	 * Default addresses are mostly useless for OBIO.
	 * The address assignments are fixed for all time,
	 * so our config files might as well reflect that.
	 */
	if (cf->cf_paddr != ca->ca_paddr)
		return 0;

	/* Now call the match function of the potential child. */
	submatch = cf->cf_attach->ca_match;
	if (submatch == NULL)
		panic("obio_submatch: no match function for: %s",
			  cf->cf_driver->cd_name);

	return ((*submatch)(parent, vcf, aux));
}


/*****************************************************************/

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

static void
save_prom_mappings()
{
	vm_offset_t pa, segva, pgva;
	int pte, sme, i;

	segva = (vm_offset_t)MONSTART;
	while (segva < (vm_offset_t)MONEND) {
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
						prom_mappings[i] = (caddr_t)pgva;
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

static void
make_required_mappings()
{
	vm_offset_t *rmp;

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
void
obio_init()
{
	save_prom_mappings();
	make_required_mappings();
}

caddr_t
obio_alloc(obio_addr, obio_size)
	int obio_addr, obio_size;
{
	caddr_t cp;

	cp = obio_find_mapping((vm_offset_t)obio_addr, obio_size);
	if (cp) return (cp);

	cp = bus_mapin(BUS_OBIO, obio_addr, obio_size);
	return (cp);
}
