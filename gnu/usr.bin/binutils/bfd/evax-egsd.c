/* evax-egsd.c -- BFD back-end for ALPHA EVAX (openVMS/AXP) files.
   Copyright 1996 Free Software Foundation Inc.

   go and read the openVMS linker manual (esp. appendix B)
   if you don't know what's going on here :-)

   Written by Klaus Kämpf (kkaempf@progis.de)
   of proGIS Softwareentwicklung, Aachen, Germany

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#include <stdio.h>
#include <ctype.h>

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"

#include "evax.h"

/*-----------------------------------------------------------------------------*/

/* sections every evax object file has  */

#define EVAX_ABS_NAME		"$ABS$"
#define EVAX_CODE_NAME		"$CODE$"
#define EVAX_LINK_NAME		"$LINK$"
#define EVAX_DATA_NAME		"$DATA$"
#define EVAX_BSS_NAME		"$BSS$"
#define EVAX_READONLY_NAME	"$READONLY$"
#define EVAX_LITERAL_NAME	"$LITERAL$"

struct sec_flags_struct {
  char *name;			/* name of section */
  int eflags_always;
  flagword flags_always;	/* flags we set always */
  int eflags_hassize;
  flagword flags_hassize;	/* flags we set if the section has a size > 0 */
};

/* just a dummy flag array since i don't understand it yet  */

static struct sec_flags_struct evax_section_flags[] = {
  { EVAX_ABS_NAME,
	(EGPS_S_V_SHR),
	(SEC_DATA),
	(EGPS_S_V_SHR),
	(SEC_IN_MEMORY|SEC_DATA|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_LOAD) },
  { EVAX_CODE_NAME,
	(EGPS_S_V_PIC|EGPS_S_V_REL|EGPS_S_V_SHR|EGPS_S_V_EXE),
	(SEC_CODE),
	(EGPS_S_V_PIC|EGPS_S_V_REL|EGPS_S_V_SHR|EGPS_S_V_EXE),
	(SEC_IN_MEMORY|SEC_CODE|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_LOAD) },
  { EVAX_LINK_NAME,
	(EGPS_S_V_REL|EGPS_S_V_RD),
	(SEC_DATA|SEC_READONLY),
	(EGPS_S_V_REL|EGPS_S_V_RD),
	(SEC_IN_MEMORY|SEC_DATA|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_READONLY|SEC_LOAD) },
  { EVAX_DATA_NAME,
	(EGPS_S_V_REL|EGPS_S_V_RD|EGPS_S_V_WRT),
	(SEC_DATA),
	(EGPS_S_V_REL|EGPS_S_V_RD|EGPS_S_V_WRT),
	(SEC_IN_MEMORY|SEC_DATA|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_LOAD) },
  { EVAX_BSS_NAME,
	(EGPS_S_V_REL|EGPS_S_V_RD|EGPS_S_V_WRT|EGPS_S_V_NOMOD),
	(SEC_NO_FLAGS),
	(EGPS_S_V_REL|EGPS_S_V_RD|EGPS_S_V_WRT|EGPS_S_V_NOMOD),
	(SEC_IN_MEMORY|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_LOAD) },
  { EVAX_READONLY_NAME,
	(EGPS_S_V_PIC|EGPS_S_V_REL|EGPS_S_V_SHR|EGPS_S_V_RD),
	(SEC_DATA|SEC_READONLY),
	(EGPS_S_V_PIC|EGPS_S_V_REL|EGPS_S_V_SHR|EGPS_S_V_RD),
	(SEC_IN_MEMORY|SEC_DATA|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_READONLY|SEC_LOAD) },
  { EVAX_LITERAL_NAME,
	(EGPS_S_V_PIC|EGPS_S_V_REL|EGPS_S_V_SHR|EGPS_S_V_RD),
	(SEC_DATA|SEC_READONLY),
	(EGPS_S_V_PIC|EGPS_S_V_REL|EGPS_S_V_SHR|EGPS_S_V_RD),
	(SEC_IN_MEMORY|SEC_DATA|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_READONLY|SEC_LOAD) },
  { NULL,
	(EGPS_S_V_REL|EGPS_S_V_RD|EGPS_S_V_WRT),
	(SEC_DATA),
	(EGPS_S_V_REL|EGPS_S_V_RD|EGPS_S_V_WRT),
	(SEC_IN_MEMORY|SEC_DATA|SEC_HAS_CONTENTS|SEC_ALLOC|SEC_LOAD) }
};

/* Retrieve bfd section flags by name and size  */

static flagword
evax_secflag_by_name(name, size)
     char *name;
     int size;
{
  int i = 0;

  while (evax_section_flags[i].name != NULL)
    {
      if (strcmp (name, evax_section_flags[i].name) == 0)
	{
	  if (size > 0)
	    return evax_section_flags[i].flags_hassize;
	  else
	    return evax_section_flags[i].flags_always;
	}
      i++;
    }
  if (size > 0)
    return evax_section_flags[i].flags_hassize;
  return evax_section_flags[i].flags_always;
}


/* Retrieve evax section flags by name and size  */

static flagword
evax_esecflag_by_name(name, size)
     char *name;
     int size;
{
  int i = 0;

  while (evax_section_flags[i].name != NULL)
    {
      if (strcmp (name, evax_section_flags[i].name) == 0)
	{
	  if (size > 0)
	    return evax_section_flags[i].eflags_hassize;
	  else
	    return evax_section_flags[i].eflags_always;
	}
      i++;
    }
  if (size > 0)
    return evax_section_flags[i].eflags_hassize;
  return evax_section_flags[i].eflags_always;
}

/*-----------------------------------------------------------------------------*/
#if EVAX_DEBUG
/* debug */

struct flagdescstruct { char *name; flagword value; };

/* Convert flag to printable string  */

static char *
flag2str(flagdesc, flags)
     struct flagdescstruct *flagdesc;
     flagword flags;
{

  static char res[64];
  int next = 0;

  res[0] = 0;
  while (flagdesc->name != NULL)
    {
      if ((flags & flagdesc->value) != 0)
	{
	  if (next)
	    strcat(res, ",");
	  else
	    next = 1;
	  strcat (res, flagdesc->name);
	}
      flagdesc++;
    }
  return res;
}
#endif

/*-----------------------------------------------------------------------------*/
/* input routines */

/* Process EGSD record
   return 0 on success, -1 on error  */

int
_bfd_evax_slurp_egsd (abfd)
     bfd *abfd;
{
#if EVAX_DEBUG
  static struct flagdescstruct gpsflagdesc[] =
  {
    { "PIC", 0x0001 },
    { "LIB", 0x0002 },
    { "OVR", 0x0004 },
    { "REL", 0x0008 },
    { "GBL", 0x0010 },
    { "SHR", 0x0020 },
    { "EXE", 0x0040 },
    { "RD",  0x0080 },
    { "WRT", 0x0100 },
    { "VEC", 0x0200 },
    { "NOMOD", 0x0400 },
    { "COM", 0x0800 },
    { NULL, 0 }
  };

  static struct flagdescstruct gsyflagdesc[] =
  {
    { "WEAK", 0x0001 },
    { "DEF",  0x0002 },
    { "UNI",  0x0004 },
    { "REL",  0x0008 },
    { "COMM", 0x0010 },
    { "VECEP", 0x0020 },
    { "NORM", 0x0040 },
    { NULL, 0 }
  };
#endif

  int gsd_type, gsd_size;
  asection *section;
  unsigned char *evax_rec;
  flagword new_flags, old_flags;
  char *name;
  asymbol *symbol;
  evax_symbol_entry *entry;
  unsigned long base_addr;
  unsigned long align_addr;

#if EVAX_DEBUG
  evax_debug (2, "EGSD\n");
#endif

  PRIV(evax_rec) += 8;	/* skip type, size, l_temp */
  PRIV(rec_size) -= 8;

  /* calculate base address for each section  */
  base_addr = 0L;

  abfd->symcount = 0;

  while (PRIV(rec_size) > 0)
    {
      evax_rec = PRIV(evax_rec);
      _bfd_evax_get_header_values (abfd, evax_rec, &gsd_type, &gsd_size);
      switch (gsd_type)
	{
	case EGSD_S_C_PSC:
	  {
	    /* program section definition  */

	    name = _bfd_evax_save_counted_string ((char *)evax_rec+12);
	    section = bfd_make_section (abfd, name);
	    if (!section)
	      return -1;
	    old_flags = bfd_getl16 (evax_rec + 6);
	    section->_raw_size = bfd_getl32 (evax_rec + 8);	/* allocation */
	    new_flags = evax_secflag_by_name (name, (int) section->_raw_size);
	    if (old_flags & EGPS_S_V_REL)
	      new_flags |= SEC_RELOC;
	    if (!bfd_set_section_flags (abfd, section, new_flags))
	      return -1;
	    section->alignment_power = evax_rec[4];
	    align_addr = (1 << section->alignment_power);
	    if ((base_addr % align_addr) != 0)
	      base_addr += (align_addr - (base_addr % align_addr));
	    section->vma = (bfd_vma)base_addr;
	    base_addr += section->_raw_size;
	    section->contents = ((unsigned char *)
				 bfd_malloc (section->_raw_size));
	    if (section->contents == NULL)
	      return -1;
	    memset (section->contents, 0, (size_t) section->_raw_size);
	    section->_cooked_size = section->_raw_size;
#if EVAX_DEBUG
	    evax_debug(3, "egsd psc %d (%s, flags %04x=%s) ",
		       section->index, name, old_flags, flag2str(gpsflagdesc, old_flags));
	    evax_debug(3, "%d bytes at 0x%08lx (mem %p)\n",
		       section->_raw_size, section->vma, section->contents);
#endif
	  }
	  break;

	case EGSD_S_C_SYM:
	  {
	    /* symbol specification (definition or reference)  */

            symbol = _bfd_evax_make_empty_symbol (abfd);
            if (symbol == 0)
              return -1;

	    old_flags = bfd_getl16 (evax_rec + 6);
	    new_flags = BSF_NO_FLAGS;

	    if (old_flags & EGSY_S_V_WEAK)
	      new_flags |= BSF_WEAK;

	    if (evax_rec[6] & EGSY_S_V_DEF)	/* symbol definition */
	      {
		symbol->name =
		  _bfd_evax_save_counted_string ((char *)evax_rec+32);
		if (old_flags & EGSY_S_V_NORM)
		  {         /* proc def */
		    new_flags |= BSF_FUNCTION;
		  }
		symbol->value = bfd_getl64 (evax_rec+8);
		symbol->section = (asection *)((unsigned long) bfd_getl32 (evax_rec+28));
#if EVAX_DEBUG
		evax_debug(3, "egsd sym def #%d (%s, %d, %04x=%s)\n", abfd->symcount,
			   symbol->name, (int)symbol->section, old_flags, flag2str(gsyflagdesc, old_flags));
#endif
	      }
	    else	/* symbol reference */
	      {
                symbol->name =
		  _bfd_evax_save_counted_string ((char *)evax_rec+8);
#if EVAX_DEBUG
		evax_debug(3, "egsd sym ref #%d (%s, %04x=%s)\n", abfd->symcount,
			   symbol->name, old_flags, flag2str(gsyflagdesc, old_flags));
#endif
                symbol->section = bfd_make_section (abfd, BFD_UND_SECTION_NAME);
	      }

	    symbol->flags = new_flags;

	    /* save symbol in evax_symbol_table  */

            entry = (evax_symbol_entry *) bfd_hash_lookup (PRIV(evax_symbol_table), symbol->name, true, false);
	    if (entry == (evax_symbol_entry *)NULL)
	      {
		bfd_set_error (bfd_error_no_memory);
		return -1;
	      }
	    if (entry->symbol != (asymbol *)NULL)
	      {					/* FIXME ?, DEC C generates this */
#if EVAX_DEBUG
		evax_debug(3, "EGSD_S_C_SYM: duplicate \"%s\"\n", symbol->name);
#endif
	      }
	    else
	      {
		entry->symbol = symbol;
		PRIV(egsd_sym_count)++;
		abfd->symcount++;
	      }
	  }
	  break;

	case EGSD_S_C_IDC:
	  break;

	default:
	  (*_bfd_error_handler) ("unknown egsd subtype %d", gsd_type);
	  bfd_set_error (bfd_error_bad_value);
	  return -1;

	} /* switch */

      PRIV(rec_size) -= gsd_size;
      PRIV(evax_rec) += gsd_size;

    } /* while (recsize > 0) */

  if (abfd->symcount > 0)
    abfd->flags |= HAS_SYMS;

  return 0;
}

/*-----------------------------------------------------------------------------*/
/* output routines */

/* Write section and symbol directory of bfd abfd  */

int
_bfd_evax_write_egsd (abfd)
     bfd *abfd;
{
  asection *section;
  asymbol *symbol;
  int symnum;
  int last_index = -1;
  char dummy_name[10];
  char *sname;
  flagword new_flags, old_flags;
  char uname[200];
  char *nptr, *uptr;

#if EVAX_DEBUG
  evax_debug (2, "evax_write_egsd(%p)\n", abfd);
#endif

  /* output sections  */

  section = abfd->sections;
#if EVAX_DEBUG
  evax_debug (3, "%d sections found\n", abfd->section_count);
#endif

  /* egsd is quadword aligned  */

  _bfd_evax_output_alignment (abfd, 8);

  _bfd_evax_output_begin (abfd, EOBJ_S_C_EGSD, -1);
  _bfd_evax_output_long (abfd, 0);
  _bfd_evax_output_push (abfd);		/* prepare output for subrecords */

  while (section != 0)
    {
#if EVAX_DEBUG
  evax_debug (3, "Section #%d %s, %d bytes\n", section->index, section->name, (int)section->_raw_size);
#endif

	/* 13 bytes egsd, max 31 chars name -> should be 44 bytes */
      if (_bfd_evax_output_check (abfd, 64) < 0)
	{
	  _bfd_evax_output_pop (abfd);
	  _bfd_evax_output_end (abfd);
	  _bfd_evax_output_begin (abfd, EOBJ_S_C_EGSD, -1);
	  _bfd_evax_output_long (abfd, 0);
	  _bfd_evax_output_push (abfd);		/* prepare output for subrecords */
	}

	/* Create dummy sections to keep consecutive indices */

      while (section->index - last_index > 1)
	{
#if EVAX_DEBUG
	  evax_debug (3, "index %d, last %d\n", section->index, last_index);
#endif
	  _bfd_evax_output_begin (abfd, EGSD_S_C_PSC, -1);
	  _bfd_evax_output_short (abfd, 0);
	  _bfd_evax_output_short (abfd, 0);
	  _bfd_evax_output_long (abfd, 0);
	  sprintf (dummy_name, ".DUMMY%02d", last_index);
	  _bfd_evax_output_counted (abfd, dummy_name);
	  _bfd_evax_output_flush (abfd);
	  last_index++;
	}

      /* Don't know if this is neccesary for the linker but for now it keeps
	 evax_slurp_egsd happy  */

      sname = (char *)section->name;
      if (*sname == '.')
	{
	  sname++;
	  if ((*sname == 't') && (strcmp (sname, "text") == 0))
	    sname = EVAX_CODE_NAME;
	  else if ((*sname == 'd') && (strcmp (sname, "data") == 0))
	    sname = EVAX_DATA_NAME;
	  else if ((*sname == 'b') && (strcmp (sname, "bss") == 0))
	    sname = EVAX_BSS_NAME;
	  else if ((*sname == 'l') && (strcmp (sname, "link") == 0))
	    sname = EVAX_LINK_NAME;
	  else if ((*sname == 'r') && (strcmp (sname, "rdata") == 0))
	    sname = EVAX_READONLY_NAME;
	  else if ((*sname == 'l') && (strcmp (sname, "literal") == 0))
	    sname = EVAX_LITERAL_NAME;
	}

      _bfd_evax_output_begin (abfd, EGSD_S_C_PSC, -1);
      _bfd_evax_output_short (abfd, section->alignment_power & 0xff);
      _bfd_evax_output_short (abfd,
			      evax_esecflag_by_name (sname,
						     section->_raw_size));
      _bfd_evax_output_long (abfd, section->_raw_size);
      _bfd_evax_output_counted (abfd, sname);
      _bfd_evax_output_flush (abfd);

      last_index = section->index;
      section = section->next;
    }

  /* output symbols  */

#if EVAX_DEBUG
  evax_debug (3, "%d symbols found\n", abfd->symcount);
#endif

  bfd_set_start_address (abfd, (bfd_vma)-1);

  for (symnum = 0; symnum < abfd->symcount; symnum++)
    {

      symbol = abfd->outsymbols[symnum];
      if (*(symbol->name) == '_')
	{
	  if (strcmp (symbol->name, "__main") == 0)
	    bfd_set_start_address (abfd, (bfd_vma)symbol->value);
	}
      old_flags = symbol->flags;

      if (old_flags & BSF_FILE)
	continue;

      if (((old_flags & BSF_GLOBAL) == 0)		/* not xdef */
	  && (!bfd_is_und_section (symbol->section)))	/* and not xref */
	continue;					/* dont output */

      /* 13 bytes egsd, max 64 chars name -> should be 77 bytes  */

      if (_bfd_evax_output_check (abfd, 80) < 0)
	{
	  _bfd_evax_output_pop (abfd);
	  _bfd_evax_output_end (abfd);
	  _bfd_evax_output_begin (abfd, EOBJ_S_C_EGSD, -1);
	  _bfd_evax_output_long (abfd, 0);
	  _bfd_evax_output_push (abfd);		/* prepare output for subrecords */
	}

      _bfd_evax_output_begin (abfd, EGSD_S_C_SYM, -1);

      _bfd_evax_output_short (abfd, 0);			/* data type, alignment */

      new_flags = 0;
      if (old_flags & BSF_WEAK)
	new_flags |= EGSY_S_V_WEAK;
      if (old_flags & BSF_FUNCTION)
	{
	  new_flags |= EGSY_S_V_NORM;
	  new_flags |= EGSY_S_V_REL;
	}
      if (old_flags & BSF_GLOBAL)
	{
	  new_flags |= EGSY_S_V_DEF;
	  if (!bfd_is_abs_section (symbol->section))
	    new_flags |= EGSY_S_V_REL;
	}
      _bfd_evax_output_short (abfd, new_flags);

      if (old_flags & BSF_GLOBAL)			/* symbol definition */
	{
	  if (old_flags & BSF_FUNCTION)
	    {
	      _bfd_evax_output_quad (abfd, symbol->value);
	      _bfd_evax_output_quad (abfd,
				     ((asymbol *)(symbol->udata.p))->value);
	      _bfd_evax_output_long (abfd,
				     (((asymbol *)(symbol->udata.p))
				      ->section->index));
	      _bfd_evax_output_long (abfd, symbol->section->index);
	    }
	  else
	    {
	      _bfd_evax_output_quad (abfd, symbol->value);	/* L_VALUE */
	      _bfd_evax_output_quad (abfd, 0);			/* L_CODE_ADDRESS */
	      _bfd_evax_output_long (abfd, 0);			/* L_CA_PSINDX */
	      _bfd_evax_output_long (abfd, symbol->section->index);/* L_PSINDX, FIXME */
	    }
	}
      _bfd_evax_output_counted (abfd, _bfd_evax_case_hack_symbol (abfd, symbol->name));

      _bfd_evax_output_flush (abfd);

    }

  _bfd_evax_output_alignment (abfd, 8);
  _bfd_evax_output_pop (abfd);
  _bfd_evax_output_end (abfd); 

  return 0;
}
