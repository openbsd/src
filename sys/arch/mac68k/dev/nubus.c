/*	$OpenBSD: nubus.c,v 1.29 2004/11/28 14:04:24 miod Exp $	*/
/*	$NetBSD: nubus.c,v 1.35 1997/04/22 20:20:32 scottr Exp $	*/

/*
 * Copyright (c) 1995, 1996 Allen Briggs.  All rights reserved.
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
 *	This product includes software developed by Allen Briggs.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/vmparam.h>
#include <machine/param.h>
#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/viareg.h>

#include "nubus.h"

#ifdef DEBUG
static int	nubus_debug = 0x01;
#define NDB_PROBE	0x1
#define NDB_FOLLOW	0x2
#define NDB_ARITH	0x4
#endif

static int	nubus_print(void *, const char *);
static int	nubus_match(struct device *, void *, void *);
static void	nubus_attach(struct device *, struct device *, void *);
int		nubus_video_resource(int);

static int	probe_slot(int slot, nubus_slot *fmt);
static u_long	IncPtr(nubus_slot *fmt, u_long base, long amt);
static u_long	nubus_calc_CRC(nubus_slot *fmt);
static u_char	GetByte(nubus_slot *fmt, u_long ptr);
#ifdef notyet
/* unused */ static u_short	GetWord(nubus_slot *fmt, u_long ptr);
#endif
static u_long	GetLong(nubus_slot *fmt, u_long ptr);

static int	nubus_peek(vm_offset_t, int);

struct cfattach nubus_ca = {
	sizeof(struct nubus_softc), nubus_match, nubus_attach
};

struct cfdriver nubus_cd = {
	NULL, "nubus", DV_DULL,
};

static int
nubus_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	static int nubus_matched = 0;

	/* Allow only one instance. */
	if (nubus_matched)
		return (0);

	nubus_matched = 1;
	return (1);
}

static void
nubus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct nubus_attach_args na_args;
	nubus_slot fmtblock;
	nubus_dir dir;
	nubus_dirent dirent;
	nubus_type slottype;
	u_long entry;
	int i, rsrcid;

  	printf("\n");
  
  	for (i = NUBUS_MIN_SLOT; i <= NUBUS_MAX_SLOT; i++) {
		if (probe_slot(i, &fmtblock) <= 0)
			continue;

		rsrcid = 0x80;

		nubus_get_main_dir(&fmtblock, &dir);

		/*
		 * Get the resource for the first function on the card.
		 * This is assumed to be at resource ID 0x80.  If we can
		 * not find this entry (as we can not on some video cards),
		 * check to see if we can get a different ID from the list
		 * of video resources given to us by the booter.  If that
		 * doesn't work either, take the first resource following
		 * the board resource.
		 */
		if (nubus_find_rsrc(&fmtblock, &dir, rsrcid, &dirent) <= 0) {
			if ((rsrcid = nubus_video_resource(i)) == -1) {
				/*
				 * Since nubus_find_rsrc failed, the directory
				 * is back at its base.
				 */
				entry = dir.curr_ent;

				/*
				 * All nubus cards should have a board
				 * resource, but be sure that's what it
				 * is before we skip it.
				 */
				rsrcid = GetByte(&fmtblock, entry);
				if (rsrcid == 0x1)
					entry =
					    IncPtr(&fmtblock, dir.curr_ent, 4);

				rsrcid = GetByte(&fmtblock, entry);
#ifdef DEBUG
				if (nubus_debug & NDB_FOLLOW)
					printf("\tUsing rsrc 0x%x.\n", rsrcid);
#endif
				if (rsrcid == 0xff)
					continue;	/* end of chain */
			}
			/*
			 * Try to find the resource passed by the booter
			 * or the one we just tracked down.
			 */
			if (nubus_find_rsrc(&fmtblock, &dir,
					    rsrcid, &dirent) <= 0) {
				continue;
			}
		}

		nubus_get_dir_from_rsrc(&fmtblock, &dirent, &dir);

		if (nubus_find_rsrc(&fmtblock, &dir, NUBUS_RSRC_TYPE,
		    &dirent) <= 0)
			continue;

		if (nubus_get_ind_data(&fmtblock, &dirent,
		    (caddr_t) &slottype, sizeof(nubus_type)) <= 0)
			continue;

		/*
		 * If this is a display card, try to pull out the correct
		 * display mode as passed by the booter.
		 */
		if (slottype.category == NUBUS_CATEGORY_DISPLAY) {
			int	r;

			if ((r = nubus_video_resource(i)) != -1) {

				nubus_get_main_dir(&fmtblock, &dir);

				if (nubus_find_rsrc(&fmtblock, &dir,
						    r, &dirent) <= 0)
					continue;

				nubus_get_dir_from_rsrc(&fmtblock,
							&dirent, &dir);

				if (nubus_find_rsrc(&fmtblock, &dir,
						NUBUS_RSRC_TYPE, &dirent) <= 0)
					continue;

				if (nubus_get_ind_data(&fmtblock, &dirent,
						(caddr_t) &slottype,
						sizeof(nubus_type)) <= 0)
					continue;

				rsrcid = r;
			}
		}

		na_args.slot = i;
		na_args.rsrcid = rsrcid;
		na_args.category = slottype.category;
		na_args.type = slottype.type;
		na_args.drsw = slottype.drsw;
		na_args.drhw = slottype.drhw;
		na_args.fmt = &fmtblock;

		config_found(self, &na_args, nubus_print);
  	}

	/*
	 * enable nubus interrupts here.
	 */
	enable_nubus_intr();
}

static int
nubus_print(aux, name)
	void *aux;
	const char *name;
{
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;

	if (name) {
		printf("%s: slot %x: %s", name, na->fmt->slot,
		    nubus_get_card_name(na->fmt));
		printf(" (Vendor: %s,",
		    nubus_get_vendor(na->fmt, NUBUS_RSRC_VEND_ID));
		printf(" Part: %s",
		    nubus_get_vendor(na->fmt, NUBUS_RSRC_VEND_PART));
#ifdef DIAGNOSTIC
		printf(" Type: %04x %04x %04x %04x",
		    na->category, na->type, na->drsw, na->drhw);
#endif
	}
	return (UNCONF);
}

int
nubus_video_resource(slot)
	int slot;
{
	extern u_int16_t mac68k_vrsrc_vec[];
	int i;

	for (i = 0 ; i < 6 ; i++)
		if ((mac68k_vrsrc_vec[i] & 0xff) == slot)
			return ((mac68k_vrsrc_vec[i] >> 8) & 0xff);
	return (-1);
}

/*
 * Probe a given nubus slot.  If a card is there and we can get the
 * format block from it's clutching decl. ROMs, fill the format block
 * and return non-zero.  If we can't find a card there with a valid
 * decl. ROM, return 0.
 *
 * First, we check to see if we can access the memory at the tail
 * end of the slot.  If so, then we check for a bytelanes byte.  We
 * could probably just return a failure status if we bus error on
 * the first try, but there really is little reason not to go ahead
 * and check the other three locations in case there's a weird card
 * out there.
 *
 * Checking for a card involves locating the "bytelanes" byte which
 * tells us how to interpret the declaration ROM's data.  The format
 * block is at the top of the card's standard memory space and the
 * bytelanes byte is at the end of that block.
 *
 * After some inspection of the bytelanes byte, it appears that it
 * takes the form 0xXY where Y is a bitmask of the bytelanes in use
 * and X is a bitmask of the lanes to ignore.  Hence, (X ^ Y) == 0
 * and (less obviously), Y will have the upper N bits clear if it is
 * found N bytes from the last possible location.  Both that and
 * the exclusive-or check are made.
 *
 * If a valid
 */
static u_char	nbits[]={0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
static int
probe_slot(slot, fmt)
	int		slot;
	nubus_slot	*fmt;
{
	caddr_t		rom_probe;
	vm_offset_t	hdr;
#ifdef DEBUG
	vm_offset_t	pa;
#endif
	u_int		data;
	int		hdr_size, i;

	fmt->bytelanes = 0;
	fmt->slot = (u_long)slot;

	rom_probe = (caddr_t) (NUBUS_SLOT2PA(fmt->slot) + NBMEMSIZE);

#ifdef DEBUG
	if (nubus_debug & NDB_PROBE) {
		pmap_extract(pmap_kernel(), (vm_offset_t) rom_probe - 1, &pa);
		printf("probing slot %d, first probe at %p (PA %lx).\n",
		    slot, rom_probe - 1, pa);
	}
#endif

	for (i = 4; i && (fmt->bytelanes == 0); i--) {

		rom_probe--;

		data = nubus_peek((vm_offset_t) rom_probe, 1);

		if (data == (u_int) -1)
			continue;

		if (data == 0)
			continue;

		if (   ((((data & 0xf0) >> 4) ^ (data & 0x0f)) == 0x0f)
		    && ((data & 0x0f) < (1 << i)) ) {
			fmt->bytelanes = data;
			fmt->step = nbits[(data & 0x0f)];
		}
	}
#ifdef DEBUG
	if (nubus_debug & NDB_PROBE)
		if (fmt->bytelanes == 0)
			printf("bytelanes not found for slot 0x%x.\n", slot);
#endif

	if (fmt->bytelanes == 0)
		return 0;

#ifdef DEBUG
	if (nubus_debug & NDB_PROBE)
		printf("bytelanes of 0x%x found for slot 0x%x.\n",
			fmt->bytelanes, slot);
#endif

	hdr_size = 20;

	/*
	 * Go ahead and attempt to load format header.
	 * First, we need to find the first byte beyond memory that
	 * would be valid.  This is necessary for NUBUS_ROM_offset()
	 * to work.
	 */
	hdr = (vm_offset_t)
		nubus_mapin(NUBUS_SLOT2PA(fmt->slot), NBMEMSIZE);
	if (hdr == NULL) {
		printf("Failed to map %d bytes for NuBUS slot %d probe.  ",
			NBMEMSIZE, fmt->slot);
		printf("Physical slot address %x\n",
			(unsigned int) NUBUS_SLOT2PA(fmt->slot));
	}
	fmt->virtual_base = hdr;
	hdr += NBMEMSIZE;

	i = 0x10 | (fmt->bytelanes & 0x0f);
	while ((i & 1) == 0) {
		hdr++;
		i >>= 1;
	}
	fmt->top = hdr;
	hdr = IncPtr(fmt, hdr, -hdr_size);
#ifdef DEBUG
	if (nubus_debug & NDB_PROBE)
		printf("fmt->top is 0x%lx, that minus 0x%x puts us at 0x%lx.\n",
			fmt->top, hdr_size, hdr);
#if 0
	for (i=1 ; i < 8 ; i++) {
		printf("0x%x - 0x%x = 0x%x, + 0x%x = 0x%x.\n",
			hdr, i, IncPtr(fmt, hdr, -i),
			     i, IncPtr(fmt, hdr,  i));
	}
#endif
#endif

	fmt->directory_offset = 0xff000000 | GetLong(fmt, hdr);
	hdr = IncPtr(fmt, hdr, 4);
	fmt->length = GetLong(fmt, hdr);
	hdr = IncPtr(fmt, hdr, 4);
	fmt->crc = GetLong(fmt, hdr);
	hdr = IncPtr(fmt, hdr, 4);
	fmt->revision_level = GetByte(fmt, hdr);
	hdr = IncPtr(fmt, hdr, 1);
	fmt->format = GetByte(fmt, hdr);
	hdr = IncPtr(fmt, hdr, 1);
	fmt->test_pattern = GetLong(fmt, hdr);

#ifdef DEBUG
	if (nubus_debug & NDB_PROBE) {
		printf("Directory offset 0x%x\t", fmt->directory_offset);
		printf("Length 0x%x\t", fmt->length);
		printf("CRC 0x%x\n", fmt->crc);
		printf("Revision level 0x%x\t", fmt->revision_level);
		printf("Format 0x%x\t", fmt->format);
		printf("Test Pattern 0x%x\n", fmt->test_pattern);
	}
#endif

	if ((fmt->directory_offset & 0x00ff0000) == 0) {
		printf("Invalid looking directory offset (0x%x)!\n",
			fmt->directory_offset);
		return 0;
	}
	if (fmt->test_pattern != NUBUS_ROM_TEST_PATTERN) {
		printf("Nubus--test pattern invalid:\n");
		printf("       slot 0x%x, bytelanes 0x%x?\n",
			fmt->slot, fmt->bytelanes);
		printf("       read test 0x%x, compare with 0x%x.\n",
			fmt->test_pattern, NUBUS_ROM_TEST_PATTERN);
		return 0;
	}

	/* Perform CRC */
	if (fmt->crc != nubus_calc_CRC(fmt)) {
		printf("Nubus--crc check failed, slot 0x%x.\n",
			fmt->slot);
		return 0;
	}

	return 1;
}

/*
 * Compute byte offset on card, taking into account bytelanes.
 * Base must be on a valid bytelane for this function to work.
 * Return the new address.
 *
 * XXX -- There has GOT to be a better way to do this.
 */
static u_long
IncPtr(fmt, base, amt)
	nubus_slot	*fmt;
	u_long		base;
	long		amt;
{
	u_char 	b, t;

	if (!amt)
		return base;

	if (amt < 0) {
		amt = -amt;
		b = fmt->bytelanes;
		t = (b << 4);
		b <<= (3 - (base & 0x3));
		while (amt) {
			b <<= 1;
			if (b == t)
				b = fmt->bytelanes;
			if (b & 0x08)
				amt--;
			base--;
		}
		return base;
	}

	t = (fmt->bytelanes & 0xf) | 0x10;
	b = t >> (base & 0x3);
	while (amt) {
		b >>= 1;
		if (b == 1)
			b = t;
		if (b & 1)
			amt--;
		base++;
	}

	return base;
}

static u_long
nubus_calc_CRC(fmt)
	nubus_slot	*fmt;
{
#if 0
	u_long	base, ptr, crc_loc, sum;
	int	i;

	base = fmt->top;
	crc_loc = NUBUS_ROM_offset(fmt, base, -12);
	ptr = NUBUS_ROM_offset(fmt, base, -fmt->length);

	sum = 0;
	while (ptr < base)
		roll #1, sum
		if (ptr == crc_loc) {
			roll #3, sum
			ptr = IncPtr(fmt, ptr, 3);
		} else {
			sum += GetByte(fmt, ptr);
		}
		ptr = IncPtr(fmt, ptr, 1);
	}

	return sum;
#endif
	return fmt->crc;
}

static u_char
GetByte(fmt, ptr)
	nubus_slot	*fmt;
	u_long		ptr;
{
	return *(caddr_t)ptr;
}

#ifdef notyet
/* Nothing uses this, yet */
static u_short
GetWord(fmt, ptr)
	nubus_slot	*fmt;
	u_long		ptr;
{
	u_short	s;

	s = (GetByte(fmt, ptr) << 8);
	ptr = IncPtr(fmt, ptr, 1);
	s |= GetByte(fmt, ptr);
	return s;
}
#endif

static u_long
GetLong(fmt, ptr)
	nubus_slot	*fmt;
	u_long		ptr;
{
	register u_long l;
	register int	i;

	l = 0;
	for ( i = 0; i < 4; i++) {
		l = (l << 8) | GetByte(fmt, ptr);
		ptr = IncPtr(fmt, ptr, 1);
	}
	return l;
}

void
nubus_get_main_dir(slot, dir_return)
	nubus_slot	*slot;
	nubus_dir	*dir_return;
{
#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_main_dir(0x%x, 0x%x)\n",
			(u_int) slot, (u_int) dir_return);
#endif
	dir_return->dirbase = IncPtr(slot, slot->top,
					slot->directory_offset - 20);
	dir_return->curr_ent = dir_return->dirbase;
}

int
nubus_find_rsrc(slot, dir, rsrcid, dirent_return)
	nubus_slot	*slot;
	nubus_dir	*dir;
	u_int8_t	rsrcid;
	nubus_dirent	*dirent_return;
{
	u_long		entry;
	u_char		byte;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_find_rsrc(0x%x, 0x%x, 0x%x, 0x%x)\n",
			(u_int) slot, (u_int) dir, (u_int) rsrcid,
			(u_int) dirent_return);
#endif
	if (slot->test_pattern != NUBUS_ROM_TEST_PATTERN)
		return -1;

	entry = dir->curr_ent;
	do {
		byte = GetByte(slot, entry);
#ifdef DEBUG
		if (nubus_debug & NDB_FOLLOW)
			printf("\tFound rsrc 0x%x.\n", byte);
#endif
		if (byte == rsrcid) {
			dirent_return->myloc = entry;
			dirent_return->rsrc_id = rsrcid;
			entry = GetLong(slot, entry);
			dirent_return->offset = (entry & 0x00ffffff);
			return 1;
		}
		if (byte == 0xff) {
			entry = dir->dirbase;
		} else {
			entry = IncPtr(slot, entry, 4);
		}
	} while (entry != (u_long) dir->curr_ent);
	return 0;
}

void
nubus_get_dir_from_rsrc(slot, dirent, dir_return)
	nubus_slot	*slot;
	nubus_dirent	*dirent;
	nubus_dir	*dir_return;
{
	u_long	loc;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_dir_from_rsrc(0x%x, 0x%x, 0x%x).\n",
			(u_int) slot, (u_int) dirent, (u_int) dir_return);
#endif
	if ((loc = dirent->offset) & 0x800000) {
		loc |= 0xff000000;
	}
	dir_return->dirbase = IncPtr(slot, dirent->myloc, loc);
	dir_return->curr_ent = dir_return->dirbase;
}

int
nubus_get_ind_data(slot, dirent, data_return, nbytes)
	nubus_slot *slot;
	nubus_dirent *dirent;
	caddr_t data_return;
	int nbytes;
{
	u_long	loc;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_ind_data(0x%x, 0x%x, 0x%x, %d).\n",
			(u_int) slot, (u_int) dirent, (u_int) data_return,
			nbytes);
#endif
	if ((loc = dirent->offset) & 0x800000) {
		loc |= 0xff000000;
	}
	loc = IncPtr(slot, dirent->myloc, loc);

	while (nbytes--) {
		*data_return++ = GetByte(slot, loc);
		loc = IncPtr(slot, loc, 1);
	}
	return 1;
}

int
nubus_get_c_string(slot, dirent, data_return, max_bytes)
	nubus_slot *slot;
	nubus_dirent *dirent;
	caddr_t data_return;
	int max_bytes;
{
	u_long	loc;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_c_string(0x%x, 0x%x, 0x%x, %d).\n",
			(u_int) slot, (u_int) dirent, (u_int) data_return,
			max_bytes);
#endif
	if ((loc = dirent->offset) & 0x800000) {
		loc |= 0xff000000;
	}
	loc = IncPtr(slot, dirent->myloc, loc);

	*data_return = '\0';
	while (max_bytes--) {
		if ((*data_return++ = GetByte(slot, loc)) == 0)
			return 1;
		loc = IncPtr(slot, loc, 1);
	}
	return 0;
}

static char	*huh = "???";

char *
nubus_get_vendor(slot, rsrc)
	nubus_slot	*slot;
	int		rsrc;
{
static	char		str_ret[64];
	nubus_dir	dir;
	nubus_dirent	ent;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_vendor(0x%x, 0x%x).\n", (u_int) slot, rsrc);
#endif
	nubus_get_main_dir(slot, &dir);
	if (nubus_find_rsrc(slot, &dir, 1, &ent) <= 0)
		return huh;
	nubus_get_dir_from_rsrc(slot, &ent, &dir);

	if (nubus_find_rsrc(slot, &dir, NUBUS_RSRC_VENDORINFO, &ent) <= 0)
		return huh;
	nubus_get_dir_from_rsrc(slot, &ent, &dir);

	if (nubus_find_rsrc(slot, &dir, rsrc, &ent) <= 0)
		return huh;

	nubus_get_c_string(slot, &ent, str_ret, 64);

	return str_ret;
}

char *
nubus_get_card_name(slot)
	nubus_slot	*slot;
{
static	char		name_ret[64];
	nubus_dir	dir;
	nubus_dirent	ent;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_card_name(0x%lx).\n", (u_long) slot);
#endif
	nubus_get_main_dir(slot, &dir);

	if (nubus_find_rsrc(slot, &dir, 1, &ent) <= 0)
		return huh;

	nubus_get_dir_from_rsrc(slot, &ent, &dir);

	if (nubus_find_rsrc(slot, &dir, NUBUS_RSRC_NAME, &ent) <= 0)
		return huh;

	nubus_get_c_string(slot, &ent, name_ret, 64);

	return name_ret;
}

/*
 * bus_*() functions adapted from sun3 generic "bus" support
 * by Allen Briggs.
 */

vm_offset_t tmp_vpages[1];

/*
 * Read addr with size len (1,2,4) into val.
 * If this generates a bus error, return -1
 *
 *	Create a temporary mapping,
 *	Try the access using peek_*
 *	Clean up temp. mapping
 */
static int
nubus_peek(paddr, sz)
	vm_offset_t paddr;
	int sz;
{
	int off, pte, rv;
	vm_offset_t pgva;
	caddr_t va;

	off = paddr & PGOFSET;
	paddr -= off;
	pte = (paddr & PG_FRAME) | (PG_V | PG_W | PG_CI);

	pgva = tmp_vpages[0];
	va = (caddr_t)pgva + off;

	mac68k_set_pte(pgva, pte);
	TBIS(pgva);

	/*
	 * OK, try the access using one of the assembly routines
	 * that will set pcb_onfault and catch any bus errors.
	 */
	rv = -1;
	switch (sz) {
	case 1:
		if (!badbaddr(va))
			rv = *((u_char *) va);
		break;
	case 2:
		if (!badwaddr(va))
			rv = *((u_int16_t *) va);
		break;
	case 4:
		if (!badladdr(va))
			rv = *((u_int32_t *) va);
		break;
	default:
		printf("bus_peek: invalid size=%d\n", sz);
		rv = -1;
	}

	mac68k_set_pte(pgva, PG_NV);
	TBIS(pgva);

	return rv;
}

char *
nubus_mapin(paddr, sz)
	int paddr, sz;
{
	int off, pa, pmt=0;
	vm_offset_t va, retval;

	off = paddr & PGOFSET;
	pa = paddr - off;
	sz += off;
	sz = m68k_round_page(sz);

	/* Get some kernel virtual address space. */
	va = uvm_km_valloc_wait(kernel_map, sz);
	if (va == 0)
		panic("bus_mapin");
	retval = va + off;

	/* Map it to the specified bus. */
	do {
		pmap_enter(pmap_kernel(), va, pa | pmt,
		    VM_PROT_READ|VM_PROT_WRITE, VM_PROT_READ|VM_PROT_WRITE);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((sz -= PAGE_SIZE) > 0);
	pmap_update(pmap_kernel());

	return ((char *)retval);
}
