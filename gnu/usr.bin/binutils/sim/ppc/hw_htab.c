/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _HW_HTAB_C_
#define _HW_HTAB_C_

#ifndef STATIC_INLINE_HW_HTAB
#define STATIC_INLINE_HW_HTAB STATIC_INLINE
#endif

#include "device_table.h"

#include "bfd.h"


/* DEVICE

   htab - pseudo-device describing a ppc hash table

   DESCRIPTION
   
   HTAB defines the location (in physical memory) of a PowerPC HASH
   table.

   This device uses /chosen/memory to allocate space for the hash
   table.

   PROPERTIES

   real-address = <address> (required)

   The physical address for the hash table.

   nr-bytes = <size> (required)

   The size of the hash table (in bytes) that will be created at that address

   claim = <anything> (optional)

   The presence of this property indicates that the memory should be
   claimed from the memory device before being used.

   */

/* DEVICE

   pte - pseudo-device describing a htab entry

   DESCRIPTION

   A PTE device is a child of the HTAB device.  It describes a virtual
   to physical mapping that is to be entered into the hash table.

   This device uses /chosen/memory to allocate the space that is to be
   mapped.

   PROPERTIES

   real-address = <address> (required)

   The first physical address that is to be mapped by the hash table.

   wimg = <int> (required)
   pp = <int> (required)

   Protection bits to use when creating the virtual to physical
   address map.

   claim = <anything> (optional)

   The presence of this property indicates that the memory should be
   claimed from the memory device before being used.

   virtual-address = <address> (option a)
   nr-bytes = <size> (option a)

   Option A - the corresponding virtual (ad size) that the physical
   address is to be mapped to.

   file-name = <string> (option b)

   Option B - a PowerPC executable that is to be loaded at the
   physical address above and then mapped in using the information
   found in the files header.

   */



STATIC_INLINE_HW_HTAB void
htab_decode_hash_table(device *parent,
		       unsigned32 *htaborg,
		       unsigned32 *htabmask)
{
  unsigned_word htab_ra;
  unsigned htab_nr_bytes;
  unsigned n;
  /* determine the location/size of the hash table */
  if (parent == NULL
      || strcmp(device_name(parent), "htab") != 0)
    error("devices/htab - missing htab parent device\n");
  htab_ra = device_find_integer_property(parent, "real-address");
  htab_nr_bytes = device_find_integer_property(parent, "nr-bytes");
  for (n = htab_nr_bytes; n > 1; n = n / 2) {
    if (n % 2 != 0)
      error("devices/%s - htab size 0x%x not a power of two\n",
	    device_name(parent), htab_nr_bytes);
  }
  *htaborg = htab_ra;
  *htabmask = MASKED32(htab_nr_bytes - 1, 7, 31-6);
  if ((htab_ra & INSERTED32(*htabmask, 7, 15)) != 0) {
    error("devices/%s - htaborg 0x%x not aligned to htabmask 0x%x\n",
	  device_name(parent), *htaborg, *htabmask);
  }
  DTRACE(htab, ("htab - htaborg=0x%lx htabmask=0x%lx\n",
		(unsigned long)*htaborg, (unsigned long)*htabmask));
}

STATIC_INLINE void
htab_map_page(device *me,
	      unsigned_word ra,
	      unsigned64 va,
	      unsigned wimg,
	      unsigned pp,
	      unsigned32 htaborg,
	      unsigned32 htabmask)
{
  unsigned64 vpn = va << 12;
  unsigned32 vsid = INSERTED32(EXTRACTED64(vpn, 0, 23), 0, 23);
  unsigned32 page = INSERTED32(EXTRACTED64(vpn, 24, 39), 0, 15);
  unsigned32 hash = INSERTED32(EXTRACTED32(vsid, 5, 23)
			       ^ EXTRACTED32(page, 0, 15),
			       7, 31-6);
  int h;
  for (h = 0; h < 2; h++) {
    unsigned32 pteg = (htaborg | (hash & htabmask));
    int pti;
    for (pti = 0; pti < 8; pti++, pteg += 8) {
      unsigned32 current_target_pte0;
      unsigned32 current_pte0;
      if (device_dma_read_buffer(device_parent(me),
				 &current_target_pte0,
				 0, /*space*/
				 pteg,
				 sizeof(current_target_pte0)) != 4)
	error("htab_init_callback() failed to read a pte at 0x%x\n",
	      pteg);
      current_pte0 = T2H_4(current_target_pte0);
      if (!MASKED32(current_pte0, 0, 0)) {
	/* empty pte fill it */
	unsigned32 pte0 = (MASK32(0, 0)
			   | INSERTED32(EXTRACTED32(vsid, 0, 23), 1, 24)
			   | INSERTED32(h, 25, 25)
			   | INSERTED32(EXTRACTED32(page, 0, 5), 26, 31));
	unsigned32 target_pte0 = H2T_4(pte0);
	unsigned32 pte1 = (INSERTED32(EXTRACTED32(ra, 0, 19), 0, 19)
			   | INSERTED32(wimg, 25, 28)
			   | INSERTED32(pp, 30, 31));
	unsigned32 target_pte1 = H2T_4(pte1);
	if (device_dma_write_buffer(device_parent(me),
				    &target_pte0,
				    0, /*space*/
				    pteg,
				    sizeof(target_pte0),
				    1/*ro?*/) != 4
	    || device_dma_write_buffer(device_parent(me),
				       &target_pte1,
				       0, /*space*/
				       pteg + 4,
				       sizeof(target_pte1),
				       1/*ro?*/) != 4)
	  error("htab_init_callback() failed to write a pte a 0x%x\n",
		pteg);
	DTRACE(htab, ("map - va=0x%lx ra=0x%lx &pte0=0x%lx pte0=0x%lx pte1=0x%lx\n",
		      (unsigned long)va, (unsigned long)ra,
		      (unsigned long)pteg,
		      (unsigned long)pte0, (unsigned long)pte1));
	return;
      }
    }
    /* re-hash */
    hash = MASKED32(~hash, 0, 18);
  }
}

static unsigned_word
claim_memory(device *me,
	     device_instance *memory,
	     unsigned_word ra,
	     unsigned_word size)
{
  unsigned32 args[3];
  unsigned32 results[1];
  int status;
  args[0] = 0; /* alignment */
  args[1] = size;
  args[2] = ra;
  status = device_instance_call_method(memory, "claim", 3, args, 1, results);
  if (status != 0)
    device_error(me, "failed to claim memory\n");
  return results[0];
}

STATIC_INLINE_HW_HTAB void
htab_map_region(device *me,
		device_instance *memory,
		unsigned_word pte_ra,
		unsigned_word pte_va,
		unsigned nr_bytes,
		unsigned wimg,
		unsigned pp,
		unsigned32 htaborg,
		unsigned32 htabmask)
{
  unsigned_word ra;
  unsigned64 va;
  /* claim the memory */
  if (memory != NULL)
    claim_memory(me, memory, pte_ra, nr_bytes);
  /* go through all pages and create a pte for each */
  for (ra = pte_ra, va = (signed_word)pte_va;
       ra < pte_ra + nr_bytes;
       ra += 0x1000, va += 0x1000) {
    htab_map_page(me, ra, va, wimg, pp, htaborg, htabmask);
  }
}
  
typedef struct _htab_binary_sizes {
  unsigned_word text_ra;
  unsigned_word text_base;
  unsigned_word text_bound;
  unsigned_word data_ra;
  unsigned_word data_base;
  unsigned data_bound;
  device *me;
} htab_binary_sizes;

STATIC_INLINE_HW_HTAB void
htab_sum_binary(bfd *abfd,
		sec_ptr sec,
		PTR data)
{
  htab_binary_sizes *sizes = (htab_binary_sizes*)data;
  unsigned_word size = bfd_get_section_size_before_reloc (sec);
  unsigned_word vma = bfd_get_section_vma (abfd, sec);

  /* skip the section if no memory to allocate */
  if (! (bfd_get_section_flags(abfd, sec) & SEC_ALLOC))
    return;

  if ((bfd_get_section_flags (abfd, sec) & SEC_CODE)
      || (bfd_get_section_flags (abfd, sec) & SEC_READONLY)) {
    if (sizes->text_bound < vma + size)
      sizes->text_bound = ALIGN_PAGE(vma + size);
    if (sizes->text_base > vma)
      sizes->text_base = FLOOR_PAGE(vma);
  }
  else if ((bfd_get_section_flags (abfd, sec) & SEC_DATA)
	   || (bfd_get_section_flags (abfd, sec) & SEC_ALLOC)) {
    if (sizes->data_bound < vma + size)
      sizes->data_bound = ALIGN_PAGE(vma + size);
    if (sizes->data_base > vma)
      sizes->data_base = FLOOR_PAGE(vma);
  }
}

STATIC_INLINE_HW_HTAB void
htab_dma_binary(bfd *abfd,
		sec_ptr sec,
		PTR data)
{
  htab_binary_sizes *sizes = (htab_binary_sizes*)data;
  void *section_init;
  unsigned_word section_vma;
  unsigned_word section_size;
  unsigned_word section_ra;
  device *me = sizes->me;

  /* skip the section if no memory to allocate */
  if (! (bfd_get_section_flags(abfd, sec) & SEC_ALLOC))
    return;

  /* check/ignore any sections of size zero */
  section_size = bfd_get_section_size_before_reloc(sec);
  if (section_size == 0)
    return;

  /* if nothing to load, ignore this one */
  if (! (bfd_get_section_flags(abfd, sec) & SEC_LOAD))
    return;

  /* find where it is to go */
  section_vma = bfd_get_section_vma(abfd, sec);
  section_ra = 0;
  if ((bfd_get_section_flags (abfd, sec) & SEC_CODE)
      || (bfd_get_section_flags (abfd, sec) & SEC_READONLY))
    section_ra = (section_vma - sizes->text_base + sizes->text_ra);
  else if ((bfd_get_section_flags (abfd, sec) & SEC_DATA))
    section_ra = (section_vma - sizes->data_base + sizes->data_ra);
  else 
    return; /* just ignore it */

  DTRACE(htab,
	 ("load - name=%-7s vma=0x%.8lx size=%6ld ra=0x%.8lx flags=%3lx(%s%s%s%s%s )\n",
	  bfd_get_section_name(abfd, sec),
	  (long)section_vma,
	  (long)section_size,
	  (long)section_ra,
	  (long)bfd_get_section_flags(abfd, sec),
	  bfd_get_section_flags(abfd, sec) & SEC_LOAD ? " LOAD" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_CODE ? " CODE" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_DATA ? " DATA" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_ALLOC ? " ALLOC" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_READONLY ? " READONLY" : ""
	  ));

  /* dma in the sections data */
  section_init = zalloc(section_size);
  if (!bfd_get_section_contents(abfd,
				sec,
				section_init, 0,
				section_size)) {
    bfd_perror("devices/pte");
    error("devices/%s - no data loaded\n", device_name(me));
  }
  if (device_dma_write_buffer(device_parent(me),
			      section_init,
			      0 /*space*/,
			      section_ra,
			      section_size,
			      1 /*violate_read_only*/)
      != section_size)
    error("devices/%s - broken dma transfer\n", device_name(me));
  zfree(section_init); /* only free if load */
}

STATIC_INLINE_HW_HTAB void
htab_map_binary(device *me,
		device_instance *memory,
		unsigned_word ra,
		unsigned wimg,
		unsigned pp,
		const char *file_name,
		unsigned32 htaborg,
		unsigned32 htabmask)
{
  htab_binary_sizes sizes;
  bfd *image;
  sizes.text_base = -1;
  sizes.data_base = -1;
  sizes.text_bound = 0;
  sizes.data_bound = 0;
  sizes.me = me;

  /* open the file */
  image = bfd_openr(file_name, NULL);
  if (image == NULL) {
    bfd_perror("devices/pte");
    error("devices/%s - the file %s not loaded\n", device_name(me), file_name);
  }

  /* check it is valid */
  if (!bfd_check_format(image, bfd_object)) {
    bfd_close(image);
    error("devices/%s - the file %s has an invalid binary format\n",
	  device_name(me), file_name);
  }

  /* determine the size of each of the files regions */
  bfd_map_over_sections (image, htab_sum_binary, (PTR) &sizes);

  /* determine the real addresses of the sections */
  sizes.text_ra = ra;
  sizes.data_ra = ALIGN_PAGE(sizes.text_ra + 
			     (sizes.text_bound - sizes.text_base));

  DTRACE(htab, ("text map - base=0x%lx bound=0x%lx ra=0x%lx\n",
		(unsigned long)sizes.text_base,
		(unsigned long)sizes.text_bound,
		(unsigned long)sizes.text_ra));
  DTRACE(htab, ("data map - base=0x%lx bound=0x%lx ra=0x%lx\n",
		(unsigned long)sizes.data_base,
		(unsigned long)sizes.data_bound,
		(unsigned long)sizes.data_ra));

  /* set up virtual memory maps for each of the regions */
  htab_map_region(me, memory, sizes.text_ra, sizes.text_base,
		  sizes.text_bound - sizes.text_base,
		  wimg, pp,
		  htaborg, htabmask);

  htab_map_region(me, memory, sizes.data_ra, sizes.data_base,
		  sizes.data_bound - sizes.data_base,
		  wimg, pp,
		  htaborg, htabmask);

  /* dma the sections into physical memory */
  bfd_map_over_sections (image, htab_dma_binary, (PTR) &sizes);
}

static void
htab_init_data_callback(device *me)
{
  device_instance *memory = NULL;
  if (WITH_TARGET_WORD_BITSIZE != 32)
    error("devices/htab: only 32bit targets currently suported\n");

  /* find memory device */
  if (device_find_property(me, "claim") != NULL)
    memory = device_find_ihandle_property(me, "/chosen/memory");

  /* for the htab, just allocate space for it */
  if (strcmp(device_name(me), "htab") == 0) {
    unsigned_word address = device_find_integer_property(me, "real-address");
    unsigned_word length = device_find_integer_property(me, "nr-bytes");
    unsigned_word base = claim_memory(me, memory, address, length);
    if (base == -1 || base != address)
      error("htab_init_data_callback: cannot allocate hash table\n");
  }

  /* for the pte, do all the real work */
  if (strcmp(device_name(me), "pte") == 0) {
    unsigned32 htaborg;
    unsigned32 htabmask;

    htab_decode_hash_table(device_parent(me), &htaborg, &htabmask);

    if (device_find_property(me, "file-name") != NULL) {
      /* map in a binary */
      unsigned32 pte_ra = device_find_integer_property(me, "real-address");
      unsigned pte_wimg = device_find_integer_property(me, "wimg");
      unsigned pte_pp = device_find_integer_property(me, "pp");
      const char *file_name = device_find_string_property(me, "file-name");
      DTRACE(htab, ("pte - ra=0x%lx, wimg=%ld, pp=%ld, file-name=%s\n",
		    (unsigned long)pte_ra,
		    (unsigned long)pte_wimg,
		    (long)pte_pp,
		    file_name));
      htab_map_binary(me, memory, pte_ra, pte_wimg, pte_pp, file_name,
		      htaborg, htabmask);
    }
    else {
      /* handle a normal mapping definition */
      /* so that 0xff...0 is make 0xffffff00 */
      signed32 pte_va = device_find_integer_property(me, "virtual-address");
      unsigned32 pte_ra = device_find_integer_property(me, "real-address");
      unsigned pte_nr_bytes = device_find_integer_property(me, "nr-bytes");
      unsigned pte_wimg = device_find_integer_property(me, "wimg");
      unsigned pte_pp = device_find_integer_property(me, "pp");
      DTRACE(htab, ("pte - ra=0x%lx, wimg=%ld, pp=%ld, va=0x%lx, nr_bytes=%ld\n",
		    (unsigned long)pte_ra,
		    (long)pte_wimg,
		    (long)pte_pp,
		    (unsigned long)pte_va,
		    (long)pte_nr_bytes));
      htab_map_region(me, memory, pte_ra, pte_va, pte_nr_bytes, pte_wimg, pte_pp,
		      htaborg, htabmask);
    }
  }
}


static device_callbacks const htab_callbacks = {
  { NULL, htab_init_data_callback, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { passthrough_device_dma_read_buffer,
    passthrough_device_dma_write_buffer, },
  { NULL, }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode, },
};

const device_descriptor hw_htab_device_descriptor[] = {
  { "htab", NULL, &htab_callbacks },
  { "pte", NULL, &htab_callbacks }, /* yep - uses htab's table */
  { NULL },
};

#endif /* _HW_HTAB_C_ */
