/*	$OpenBSD: nubus.c,v 1.34 2008/05/19 18:42:12 miod Exp $	*/
/*	$NetBSD: nubus.c,v 1.53 2002/04/13 17:49:41 briggs Exp $	*/

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
#include <sys/malloc.h>
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

#include <mac68k/dev/nubus.h>

#ifdef DEBUG
#define NDB_PROBE	0x1
#define NDB_FOLLOW	0x2
#define NDB_ARITH	0x4
static int	nubus_debug = 0 /* | NDB_PROBE | NDB_FOLLOW | NDB_ARITH */ ;
#endif

static int	nubus_print(void *, const char *);
static int	nubus_match(struct device *, void *, void *);
static void	nubus_attach(struct device *, struct device *, void *);
static int	nubus_video_resource(int);

static int	nubus_probe_slot(bus_space_tag_t, bus_space_handle_t,
		    int, nubus_slot *);
static u_int32_t nubus_calc_CRC(bus_space_tag_t, bus_space_handle_t,
		    nubus_slot *);

static u_long	nubus_adjust_ptr(u_int8_t, u_long, long);
static u_int8_t	nubus_read_1(bus_space_tag_t, bus_space_handle_t,
		    u_int8_t, u_long);
#ifdef notyet
static u_int16_t nubus_read_2(bus_space_tag_t, bus_space_handle_t,
		    u_int8_t, u_long);
#endif
static u_int32_t nubus_read_4(bus_space_tag_t, bus_space_handle_t,
		    u_int8_t, u_long);

struct cfattach nubus_ca = {
	sizeof(struct nubus_softc), nubus_match, nubus_attach
};

struct cfdriver nubus_cd = {
	NULL, "nubus", DV_DULL,
};

static int
nubus_match(parent, cf, aux)
	struct device *parent;
	void *cf;
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
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot fmtblock;
	nubus_dir dir;
	nubus_dirent dirent;
	nubus_type slottype;
	u_long entry;
	int i, rsrcid;
	u_int8_t lanes;

	printf("\n");

	for (i = NUBUS_MIN_SLOT; i <= NUBUS_MAX_SLOT; i++) {
		na_args.slot = i;
		na_args.na_tag = bst = MAC68K_BUS_SPACE_MEM;

		if (bus_space_map(bst,
		    NUBUS_SLOT2PA(na_args.slot), NBMEMSIZE, 0, &bsh)) {
#ifdef DEBUG
			if (nubus_debug & NDB_PROBE)
				printf("%s: failed to map slot %x, "
				"address %p (in use?)\n",
				self->dv_xname, i,
				(void *)NUBUS_SLOT2PA(i));
#endif
			continue;
		}

		if (nubus_probe_slot(bst, bsh, i, &fmtblock) <= 0) {
notfound:
			bus_space_unmap(bst, bsh, NBMEMSIZE);
			continue;
		}

		rsrcid = 0x80;
		lanes = fmtblock.bytelanes;

		nubus_get_main_dir(&fmtblock, &dir);

		/*
		 * Get the resource for the first function on the card.
		 * This is assumed to be at resource ID 0x80.  If we can
		 * not find this entry (as we can not on some video cards),
		 * check to see if we can get a different ID from the list
		 * of video resources given to us by the booter.  If that
		 * doesn't work either, take the first resource following
		 * the board resource.
		 * If we only find a board resource, report that.
		 * There are cards that do not have anything else; their
		 * driver then has to match on the board resource and
		 * the card name.
		 */
		if (nubus_find_rsrc(bst, bsh,
		    &fmtblock, &dir, rsrcid, &dirent) <= 0) {
			if ((rsrcid = nubus_video_resource(i)) == -1) {
				int has_board_rsrc = 0;

				/*
				 * Since nubus_find_rsrc failed, the directory
				 * is back at its base.
				 */
				entry = dir.curr_ent;

				/*
				 * All nubus cards should have a board
				 * resource, but be sure that's what it
				 * is before we skip it, and note the fact.
				 */
				rsrcid = nubus_read_1(bst, bsh,
				    lanes, entry);
				if (rsrcid == 0x1) {
					has_board_rsrc = 1;
					entry = nubus_adjust_ptr(lanes,
					    dir.curr_ent, 4);
				}
				rsrcid = nubus_read_1(bst, bsh, lanes, entry);
				/* end of chain? */
				if (rsrcid == 0xff) {
					if (!has_board_rsrc)
						goto notfound;
					else
						rsrcid = 0x01;
				}
#ifdef DEBUG
				if (nubus_debug & NDB_FOLLOW)
					printf("\tUsing rsrc 0x%x.\n", rsrcid);
#endif
			}
			/*
			 * Try to find the resource passed by the booter
			 * or the one we just tracked down.
			 */
			if (nubus_find_rsrc(bst, bsh,
			    &fmtblock, &dir, rsrcid, &dirent) <= 0)
				goto notfound;
		}

		nubus_get_dir_from_rsrc(&fmtblock, &dirent, &dir);

		if (nubus_find_rsrc(bst, bsh,
		    &fmtblock, &dir, NUBUS_RSRC_TYPE, &dirent) <= 0)
			goto notfound;

		if (nubus_get_ind_data(bst, bsh, &fmtblock, &dirent,
		    (caddr_t)&slottype, sizeof(nubus_type)) <= 0)
			goto notfound;

		/*
		 * If this is a display card, try to pull out the correct
		 * display mode as passed by the booter.
		 */
		if (slottype.category == NUBUS_CATEGORY_DISPLAY) {
			int r;

			if ((r = nubus_video_resource(i)) != -1) {

				nubus_get_main_dir(&fmtblock, &dir);

				if (nubus_find_rsrc(bst, bsh,
				    &fmtblock, &dir, r, &dirent) <= 0)
					goto notfound;

				nubus_get_dir_from_rsrc(&fmtblock,
				    &dirent, &dir);

				if (nubus_find_rsrc(bst, bsh, &fmtblock, &dir,
				    NUBUS_RSRC_TYPE, &dirent) <= 0)
					goto notfound;

				if (nubus_get_ind_data(bst, bsh,
				    &fmtblock, &dirent, (caddr_t)&slottype,
				    sizeof(nubus_type)) <= 0)
					goto notfound;

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

		bus_space_unmap(bst, bsh, NBMEMSIZE);

		config_found(self, &na_args, nubus_print);
	}

	enable_nubus_intr();
}

static int
nubus_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct nubus_attach_args *na = (struct nubus_attach_args *)aux;
	bus_space_tag_t bst = na->na_tag;
	bus_space_handle_t bsh;

	if (pnp) {
		printf("%s slot %x", pnp, na->slot);
		if (bus_space_map(bst,
		    NUBUS_SLOT2PA(na->slot), NBMEMSIZE, 0, &bsh) == 0) {
			printf(": %s", nubus_get_card_name(bst, bsh, na->fmt));
			printf(" (Vendor: %s,", nubus_get_vendor(bst, bsh,
			    na->fmt, NUBUS_RSRC_VEND_ID));
			printf(" Part: %s)", nubus_get_vendor(bst, bsh,
			    na->fmt, NUBUS_RSRC_VEND_PART));

			bus_space_unmap(bst, bsh, NBMEMSIZE);
		}
#ifdef DIAGNOSTIC
		else
			printf(":");
		printf(" Type: %04x %04x %04x %04x",
		    na->category, na->type, na->drsw, na->drhw);
#endif
	} else {
		printf(" slot %x", na->slot);
	}
	return (UNCONF);
}

static int
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
 * format block from its clutching decl. ROMs, fill the format block
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
static u_int8_t	nbits[] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
static int
nubus_probe_slot(bst, bsh, slot, fmt)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	int slot;
	nubus_slot *fmt;
{
	u_long ofs, hdr;
	int i, j, found, hdr_size;
	u_int8_t lanes;

#ifdef DEBUG
	if (nubus_debug & NDB_PROBE)
		printf("probing slot %x\n", slot);
#endif

	/*
	 * The idea behind this glorious work of art is to probe for only
	 * valid bytelanes values at appropriate locations (see DC&D p. 159
	 * for a list).  Note the pattern:  the first 8 values are at offset
	 * 0xffffff in the slot's space; the next 4 values at 0xfffffe; the
	 * next 2 values at 0xfffffd; and the last one at 0xfffffc.
	 *
	 * The nested loops implement an efficient search of this space,
	 * probing first for a valid address, then checking for each of the
	 * valid bytelanes values at that address.
	 */
	ofs = NBMEMSIZE;
	lanes = 0xf;

	for (j = 8, found = 0; j > 0 && !found; j >>= 1) {
		ofs--;
		for (i = j; i > 0; i--, lanes--) {
			if (!mac68k_bus_space_probe(bst, bsh, ofs, 1)) {
				lanes -= i;
				break;
			}
			if (bus_space_read_1(bst, bsh, ofs) ==
			    (((~lanes & 0xf) << 4) | lanes)) {
				found = 1;
				break;
			}
		}
	}

	if (!found) {
#ifdef DEBUG
		if (nubus_debug & NDB_PROBE)
			printf("bytelanes not found for slot %x\n", slot);
#endif
		return 0;
	}

	fmt->bytelanes = lanes;
	fmt->step = nbits[(lanes & 0x0f)];
	fmt->slot = slot;	/* XXX redundant; get rid of this someday */

#ifdef DEBUG
	if (nubus_debug & NDB_PROBE)
		printf("bytelanes of 0x%x found for slot 0x%x.\n",
		    fmt->bytelanes, slot);
#endif

	/*
	 * Go ahead and attempt to load format header.
	 * First, we need to find the first byte beyond memory that
	 * would be valid.  This is necessary for NUBUS_ROM_offset()
	 * to work.
	 */
	hdr = NBMEMSIZE;
	hdr_size = 20;

	i = 0x10 | (lanes & 0x0f);
	while ((i & 1) == 0) {
		hdr++;
		i >>= 1;
	}
	fmt->top = hdr;
	hdr = nubus_adjust_ptr(lanes, hdr, -hdr_size);
#ifdef DEBUG
	if (nubus_debug & NDB_PROBE)
		printf("fmt->top is 0x%lx, that minus 0x%x puts us at 0x%lx.\n",
		    fmt->top, hdr_size, hdr);
	if (nubus_debug & NDB_ARITH)
		for (i = 1 ; i < 8 ; i++)
			printf("0x%lx - 0x%x = 0x%lx, + 0x%x = 0x%lx.\n",
			    hdr, i, nubus_adjust_ptr(lanes, hdr, -i),
			    i, nubus_adjust_ptr(lanes, hdr,  i));
#endif

	fmt->directory_offset =
	    0xff000000 | nubus_read_4(bst, bsh, lanes, hdr);
	hdr = nubus_adjust_ptr(lanes, hdr, 4);
	fmt->length = nubus_read_4(bst, bsh, lanes, hdr);
	hdr = nubus_adjust_ptr(lanes, hdr, 4);
	fmt->crc = nubus_read_4(bst, bsh, lanes, hdr);
	hdr = nubus_adjust_ptr(lanes, hdr, 4);
	fmt->revision_level = nubus_read_1(bst, bsh, lanes, hdr);
	hdr = nubus_adjust_ptr(lanes, hdr, 1);
	fmt->format = nubus_read_1(bst, bsh, lanes, hdr);
	hdr = nubus_adjust_ptr(lanes, hdr, 1);
	fmt->test_pattern = nubus_read_4(bst, bsh, lanes, hdr);

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
		printf("       slot 0x%x, bytelanes 0x%x?\n", fmt->slot, lanes);
		printf("       read test 0x%x, compare with 0x%x.\n",
		    fmt->test_pattern, NUBUS_ROM_TEST_PATTERN);
		return 0;
	}

	/* Perform CRC */
	if (fmt->crc != nubus_calc_CRC(bst, bsh, fmt)) {
		printf("Nubus--crc check failed, slot 0x%x.\n", fmt->slot);
		return 0;
	}

	return 1;
}

static u_int32_t
nubus_calc_CRC(bst, bsh, fmt)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot	*fmt;
{
#if 0
	u_long base, ptr, crc_loc;
	u_int32_t sum;
	u_int8_t lanes = fmt->bytelanes;

	base = fmt->top;
	crc_loc = NUBUS_ROM_offset(fmt, base, -12);
	ptr = NUBUS_ROM_offset(fmt, base, -fmt->length);

	sum = 0;
	while (ptr < base)
		roll #1, sum
		if (ptr == crc_loc) {
			roll #3, sum
			ptr = nubus_adjust_ptr(lanes, ptr, 3);
		} else {
			sum += nubus_read_1(bst, bsh, lanes, ptr);
		}
		ptr = nubus_adjust_ptr(lanes, ptr, 1);
	}

	return sum;
#endif
	return fmt->crc;
}

/*
 * Compute byte offset on card, taking into account bytelanes.
 * Base must be on a valid bytelane for this function to work.
 * Return the new address.
 *
 * XXX -- There has GOT to be a better way to do this.
 */
static u_long
nubus_adjust_ptr(lanes, base, amt)
	u_int8_t lanes;
	u_long base;
	long amt;
{
	u_int8_t b, t;

	if (!amt)
		return base;

	if (amt < 0) {
		amt = -amt;
		b = lanes;
		t = (b << 4);
		b <<= (3 - (base & 0x3));
		while (amt) {
			b <<= 1;
			if (b == t)
				b = lanes;
			if (b & 0x08)
				amt--;
			base--;
		}
		return base;
	}

	t = (lanes & 0xf) | 0x10;
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

static u_int8_t
nubus_read_1(bst, bsh, lanes, ofs)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t lanes;
	u_long ofs;
{
	return bus_space_read_1(bst, bsh, ofs);
}

#ifdef notyet
/* Nothing uses this, yet */
static u_int16_t
nubus_read_2(bst, bsh, lanes, ofs)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t lanes;
	u_long ofs;
{
	u_int16_t s;

	s = (nubus_read_1(bst, bsh, lanes, ofs) << 8);
	ofs = nubus_adjust_ptr(lanes, ofs, 1);
	s |= nubus_read_1(bst, bsh, lanes, ofs);
	return s;
}
#endif

static u_int32_t
nubus_read_4(bst, bsh, lanes, ofs)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t lanes;
	u_long ofs;
{
	u_int32_t l;
	int i;

	l = 0;
	for (i = 0; i < 4; i++) {
		l = (l << 8) | nubus_read_1(bst, bsh, lanes, ofs);
		ofs = nubus_adjust_ptr(lanes, ofs, 1);
	}
	return l;
}

void
nubus_get_main_dir(fmt, dir_return)
	nubus_slot *fmt;
	nubus_dir *dir_return;
{
#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_main_dir(%p, %p)\n",
		    fmt, dir_return);
#endif
	dir_return->dirbase = nubus_adjust_ptr(fmt->bytelanes, fmt->top,
	    fmt->directory_offset - 20);
	dir_return->curr_ent = dir_return->dirbase;
}

void
nubus_get_dir_from_rsrc(fmt, dirent, dir_return)
	nubus_slot *fmt;
	nubus_dirent *dirent;
	nubus_dir *dir_return;
{
	u_long loc;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_dir_from_rsrc(%p, %p, %p).\n",
		    fmt, dirent, dir_return);
#endif
	if ((loc = dirent->offset) & 0x800000) {
		loc |= 0xff000000;
	}
	dir_return->dirbase =
	    nubus_adjust_ptr(fmt->bytelanes, dirent->myloc, loc);
	dir_return->curr_ent = dir_return->dirbase;
}

int
nubus_find_rsrc(bst, bsh, fmt, dir, rsrcid, dirent_return)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot *fmt;
	nubus_dir *dir;
	u_int8_t rsrcid;
	nubus_dirent *dirent_return;
{
	u_long entry;
	u_int8_t byte, lanes = fmt->bytelanes;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_find_rsrc(%p, %p, 0x%x, %p)\n",
		    fmt, dir, rsrcid, dirent_return);
#endif
	if (fmt->test_pattern != NUBUS_ROM_TEST_PATTERN)
		return -1;

	entry = dir->curr_ent;
	do {
		byte = nubus_read_1(bst, bsh, lanes, entry);
#ifdef DEBUG
		if (nubus_debug & NDB_FOLLOW)
			printf("\tFound rsrc 0x%x.\n", byte);
#endif
		if (byte == rsrcid) {
			dirent_return->myloc = entry;
			dirent_return->rsrc_id = rsrcid;
			entry = nubus_read_4(bst, bsh, lanes, entry);
			dirent_return->offset = (entry & 0x00ffffff);
			return 1;
		}
		if (byte == 0xff) {
			entry = dir->dirbase;
		} else {
			entry = nubus_adjust_ptr(lanes, entry, 4);
		}
	} while (entry != (u_long)dir->curr_ent);
	return 0;
}

int
nubus_get_ind_data(bst, bsh, fmt, dirent, data_return, nbytes)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot *fmt;
	nubus_dirent *dirent;
	caddr_t data_return;
	int nbytes;
{
	u_long loc;
	u_int8_t lanes = fmt->bytelanes;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_ind_data(%p, %p, %p, %d).\n",
		    fmt, dirent, data_return, nbytes);
#endif
	if ((loc = dirent->offset) & 0x800000) {
		loc |= 0xff000000;
	}
	loc = nubus_adjust_ptr(lanes, dirent->myloc, loc);

	while (nbytes--) {
		*data_return++ = nubus_read_1(bst, bsh, lanes, loc);
		loc = nubus_adjust_ptr(lanes, loc, 1);
	}
	return 1;
}

int
nubus_get_c_string(bst, bsh, fmt, dirent, data_return, max_bytes)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot *fmt;
	nubus_dirent *dirent;
	caddr_t data_return;
	int max_bytes;
{
	u_long loc;
	u_int8_t lanes = fmt->bytelanes;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_c_string(%p, %p, %p, %d).\n",
		    fmt, dirent, data_return, max_bytes);
#endif
	if ((loc = dirent->offset) & 0x800000)
		loc |= 0xff000000;

	loc = nubus_adjust_ptr(lanes, dirent->myloc, loc);

	*data_return = '\0';
	while (max_bytes--) {
		if ((*data_return++ =
		    nubus_read_1(bst, bsh, lanes, loc)) == 0)
			return 1;
		loc = nubus_adjust_ptr(lanes, loc, 1);
	}
	*(data_return-1) = '\0';
	return 0;
}

/*
 * Get list of address ranges for an sMemory resource
 * ->  DC&D, p.171
 */
int
nubus_get_smem_addr_rangelist(bst, bsh, fmt, dirent, data_return)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot *fmt;
	nubus_dirent *dirent;
	caddr_t data_return;
{
	u_long loc;
	u_int8_t lanes = fmt->bytelanes;
	long blocklen;
	caddr_t blocklist;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_smem_addr_rangelist(%p, %p, %p).\n",
		    fmt, dirent, data_return);
#endif
	if ((loc = dirent->offset) & 0x800000) {
		loc |= 0xff000000;
	}
	loc = nubus_adjust_ptr(lanes, dirent->myloc, loc);

	/* Obtain the block length from the head of the list */
	blocklen = nubus_read_4(bst, bsh, lanes, loc);

	/*
	 * malloc a block of (blocklen) bytes
	 * caller must recycle block after use
	 */
	blocklist = (caddr_t)malloc(blocklen, M_TEMP, M_WAITOK);

	/* read ((blocklen - 4) / 8) (length,offset) pairs into block */
	nubus_get_ind_data(bst, bsh, fmt, dirent, blocklist, blocklen);
#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW) {
		int ii;
		nubus_smem_rangelist *rlist;

		rlist = (nubus_smem_rangelist *)blocklist;
		printf("\tblock@%p, len 0x0%X\n", rlist, rlist->length);

		for (ii=0; ii < ((blocklen - 4) / 8); ii++) {
			printf("\tRange %d: base addr 0x%X [0x%X]\n", ii,
			    rlist->range[ii].offset, rlist->range[ii].length);
		}
	}
#endif
	*(caddr_t *)data_return = blocklist;

	return 1;
}

static char	*huh = "???";

char *
nubus_get_vendor(bst, bsh, fmt, rsrc)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot *fmt;
	int rsrc;
{
	static char str_ret[64];
	nubus_dir dir;
	nubus_dirent ent;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_vendor(%p, 0x%x).\n", fmt, rsrc);
#endif
	nubus_get_main_dir(fmt, &dir);
	if (nubus_find_rsrc(bst, bsh, fmt, &dir, 1, &ent) <= 0)
		return huh;
	nubus_get_dir_from_rsrc(fmt, &ent, &dir);

	if (nubus_find_rsrc(bst, bsh, fmt, &dir, NUBUS_RSRC_VENDORINFO, &ent)
	    <= 0)
		return huh;
	nubus_get_dir_from_rsrc(fmt, &ent, &dir);

	if (nubus_find_rsrc(bst, bsh, fmt, &dir, rsrc, &ent) <= 0)
		return huh;

	nubus_get_c_string(bst, bsh, fmt, &ent, str_ret, 64);

	return str_ret;
}

char *
nubus_get_card_name(bst, bsh, fmt)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	nubus_slot *fmt;
{
	static char name_ret[64];
	nubus_dir dir;
	nubus_dirent ent;

#ifdef DEBUG
	if (nubus_debug & NDB_FOLLOW)
		printf("nubus_get_card_name(%p).\n", fmt);
#endif
	nubus_get_main_dir(fmt, &dir);

	if (nubus_find_rsrc(bst, bsh, fmt, &dir, 1, &ent) <= 0)
		return huh;

	nubus_get_dir_from_rsrc(fmt, &ent, &dir);

	if (nubus_find_rsrc(bst, bsh, fmt, &dir, NUBUS_RSRC_NAME, &ent) <= 0)
		return huh;

	nubus_get_c_string(bst, bsh, fmt, &ent, name_ret, 64);

	return name_ret;
}

#ifdef DEBUG
void
nubus_scan_slot(bst, slotno)
	bus_space_tag_t bst;
	int slotno;
{
	int i=0, state=0;
	char twirl[] = "-\\|/";
	bus_space_handle_t sc_bsh;

	if (bus_space_map(bst, NUBUS_SLOT2PA(slotno), NBMEMSIZE, 0, &sc_bsh)) {
		printf("nubus_scan_slot: failed to map slot %x\n", slotno);
		return;
	}

	printf("Scanning slot %c for accessible regions:\n",
		slotno == 9 ? '9' : slotno - 10 + 'A');
	for (i=0 ; i<NBMEMSIZE; i++) {
		if (mac68k_bus_space_probe(bst, sc_bsh, i, 1)) {
			if (state == 0) {
				printf("\t0x%x-", i);
				state = 1;
			}
		} else {
			if (state) {
				printf("0x%x\n", i);
				state = 0;
			}
		}
		if (i%100 == 0) {
			printf("%c\b", twirl[(i/100)%4]);
		}
	}
	if (state) {
		printf("0x%x\n", i);
	}
	return;
}
#endif
