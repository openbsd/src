/* evax-emh.c -- BFD back-end for ALPHA EVAX (openVMS/Alpha) files.
   Copyright 1996, 1997 Free Software Foundation, Inc.

   EMH record handling functions
   and
   EEOM record handling functions

   Written by Klaus K"ampf (kkaempf@progis.de)
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

/*---------------------------------------------------------------------------*/


/* Read & process emh record
   return 0 on success, -1 on error  */

int
_bfd_evax_slurp_emh (abfd)
     bfd *abfd;
{
  unsigned char *ptr;
  unsigned char *evax_rec;

  evax_rec = PRIV(evax_rec);

#if EVAX_DEBUG
  evax_debug(2, "EMH\n");
#endif

  switch (bfd_getl16 (evax_rec + 4))
    {

      case EMH_S_C_MHD:
	/*
	 * module header
	 */
	PRIV(emh_data).emh_b_strlvl = evax_rec[6];
	PRIV(emh_data).emh_l_arch1 = bfd_getl32 (evax_rec + 8);
	PRIV(emh_data).emh_l_arch2 = bfd_getl32 (evax_rec + 12);
	PRIV(emh_data).emh_l_recsiz = bfd_getl32 (evax_rec + 16);
	PRIV(emh_data).emh_t_name =
	  _bfd_evax_save_counted_string ((char *)evax_rec + 20);
	ptr = evax_rec + 20 + evax_rec[20] + 1;
	PRIV(emh_data).emh_t_version =
	  _bfd_evax_save_counted_string ((char *)ptr);
	ptr += *ptr + 1;
	PRIV(emh_data).emh_t_date =
	  _bfd_evax_save_sized_string ((char *)ptr, 17);

      break;

      case EMH_S_C_LNM:
	/*
	 *
	 */
	PRIV(emh_data).emh_c_lnm =
	  _bfd_evax_save_sized_string ((char *)evax_rec, PRIV(rec_length-6));
      break;

      case EMH_S_C_SRC:
	/*
	 *
	 */
	PRIV(emh_data).emh_c_src =
	  _bfd_evax_save_sized_string ((char *)evax_rec, PRIV(rec_length-6));
      break;

      case EMH_S_C_TTL:
	/*
	 *
	 */
	PRIV(emh_data).emh_c_ttl =
	  _bfd_evax_save_sized_string ((char *)evax_rec, PRIV(rec_length-6));
      break;

      case EMH_S_C_CPR:
	/*
	 *
	 */
      break;

      case EMH_S_C_MTC:
	/*
	 *
	 */
      break;

      case EMH_S_C_GTX:
	/*
	 *
	 */
      break;

      default:
	bfd_set_error (bfd_error_wrong_format);
      return -1;

    } /* switch */

  return 0;
}


/*-----------------------------------------------------------------------------*/
/* Output routines.  */


/* Manufacure a VMS like time on a unix based system.
   stolen from obj-vms.c  */

static unsigned char *
get_vms_time_string ()
{
  static unsigned char tbuf[18];
#ifndef VMS
#include <sys/types.h>
#include <time.h>

  char *pnt;
  time_t timeb;
  time (&timeb);
  pnt = ctime (&timeb);
  pnt[3] = 0;
  pnt[7] = 0;
  pnt[10] = 0;
  pnt[16] = 0;
  pnt[24] = 0;
  sprintf (tbuf, "%2s-%3s-%s %s", pnt + 8, pnt + 4, pnt + 20, pnt + 11);
#else
#include <starlet.h>
  struct
  {
    int Size;
    unsigned char *Ptr;
  } Descriptor;
  Descriptor.Size = 17;
  Descriptor.Ptr = tbuf;
  sys$asctim (0, &Descriptor, 0, 0);
#endif /* not VMS */

#if EVAX_DEBUG
  evax_debug (6, "vmstimestring:'%s'\n", tbuf);
#endif

  return tbuf;
}


/* write object header for bfd abfd  */

int
_bfd_evax_write_emh (abfd)
     bfd *abfd;
{
  asymbol *symbol;
  int symnum;
  int had_case = 0;
  int had_file = 0;


#if EVAX_DEBUG
  evax_debug (2, "evax_write_emh(%p)\n", abfd);
#endif

  _bfd_evax_output_alignment (abfd, 2);

  /* MHD */

  _bfd_evax_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_MHD);
  _bfd_evax_output_short (abfd, EOBJ_S_C_STRLVL);
  _bfd_evax_output_long (abfd, 0);
  _bfd_evax_output_long (abfd, 0);
  _bfd_evax_output_long (abfd, MAX_OUTREC_SIZE);

  if (bfd_get_filename (abfd) != 0)
    {
      /* strip path and suffix information */

      char *fname, *fout, *fptr;

      fname = strdup (bfd_get_filename (abfd));
      if (fname == 0)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return -1;
	}
      fout = strrchr (fname, ']');
      if (fout == 0)
	fout = strchr (fname, ':');
      if (fout != 0)
	fout++;
      else
	fout = fname;

      /* strip .obj suffix  */

      fptr = strrchr (fname, '.');
      if ((fptr != 0)
	  && (strcasecmp (fptr, ".OBJ") == 0))
	*fptr = 0;

      fptr = fout;
      while (*fptr != 0)
	{
	  if (islower (*fptr))
	    *fptr = toupper (*fptr);
	  fptr++;
	  if ((*fptr == ';')
	     || ((fptr - fout) > 31))
	    *fptr = 0;
	}
      _bfd_evax_output_counted (abfd, fout);
      free (fname);
    }
  else
    _bfd_evax_output_counted (abfd, "NONAME");

  _bfd_evax_output_counted (abfd, BFD_VERSION);
  _bfd_evax_output_dump (abfd, get_vms_time_string (), 17);
  _bfd_evax_output_fill (abfd, 0, 17);
  _bfd_evax_output_flush (abfd);

  /* LMN */

  _bfd_evax_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_LNM);
  _bfd_evax_output_dump (abfd, (unsigned char *)"GAS proGIS", 10);
  _bfd_evax_output_flush (abfd);

  /* SRC */

  _bfd_evax_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_SRC);

  for (symnum = 0; symnum < abfd->symcount; symnum++)
    {
      symbol = abfd->outsymbols[symnum];

      if (symbol->flags & BSF_FILE)
	{
	  char *s;

	  if (strncmp ((char *)symbol->name, "<CASE:", 6) == 0)
	    {
	      PRIV(flag_hash_long_names) = symbol->name[6] - '0';
	      PRIV(flag_show_after_trunc) = symbol->name[7] - '0';

	      if (had_file)
		break;
	      had_case = 1;
	      continue;
	    }

	  _bfd_evax_output_dump (abfd, (char *)symbol->name, strlen (symbol->name));
	  if (had_case)
	    break;
	  had_file = 1;
	}
    }

  if (symnum == abfd->symcount)
    _bfd_evax_output_dump (abfd, "noname", 6);

  _bfd_evax_output_flush (abfd);

  /* TTL */

  _bfd_evax_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_TTL);
  _bfd_evax_output_dump (abfd, (unsigned char *)"TTL", 3);
  _bfd_evax_output_flush (abfd);

  /* CPR */

  _bfd_evax_output_begin (abfd, EOBJ_S_C_EMH, EMH_S_C_CPR);
  _bfd_evax_output_dump (abfd,
			 (unsigned char *)"GNU BFD ported by Klaus Kämpf 1994-1996",
			 39);
  _bfd_evax_output_flush (abfd);

  return 0;
}

/*-----------------------------------------------------------------------------*/

/* Process EEOM record
   return 0 on success, -1 on error  */

int
_bfd_evax_slurp_eeom (abfd)
     bfd *abfd;
{
  unsigned char *evax_rec;

#if EVAX_DEBUG
  evax_debug(2, "EEOM\n");
#endif

  evax_rec = PRIV(evax_rec);

  PRIV(eeom_data).eeom_l_total_lps = bfd_getl32 (evax_rec + 4);
  PRIV(eeom_data).eeom_b_comcod = *(evax_rec + 8);
  if (PRIV(eeom_data).eeom_b_comcod > 1)
    {
      (*_bfd_error_handler) ("Object module NOT error-free !\n");
      bfd_set_error (bfd_error_bad_value);
      return -1;
    }
  PRIV(eeom_data).eeom_has_transfer = false;
  if (PRIV(rec_size) > 10)
    {
       PRIV(eeom_data).eeom_has_transfer = true;
       PRIV(eeom_data).eeom_b_tfrflg = *(evax_rec + 9);
       PRIV(eeom_data).eeom_l_psindx = bfd_getl32 (evax_rec + 12);
       PRIV(eeom_data).eeom_l_tfradr = bfd_getl32 (evax_rec + 16);

       abfd->start_address = PRIV(eeom_data).eeom_l_tfradr;
    }
  return 0;
}


/* Write eom record for bfd abfd  */

int
_bfd_evax_write_eeom (abfd)
     bfd *abfd;
{
#if EVAX_DEBUG
  evax_debug (2, "evax_write_eeom(%p)\n", abfd);
#endif

  _bfd_evax_output_begin (abfd,EOBJ_S_C_EEOM, -1);
  _bfd_evax_output_long (abfd, (unsigned long)(PRIV(evax_linkage_index) >> 1));
  _bfd_evax_output_byte (abfd, 0);	/* completion code */
  _bfd_evax_output_byte (abfd, 0);	/* fill byte */

  if (bfd_get_start_address (abfd) != (bfd_vma)-1)
    {
      asection *section;

      section = bfd_get_section_by_name (abfd, ".link");
      if (section == 0)
	{
	  bfd_set_error (bfd_error_nonrepresentable_section);
	  return -1;
	}
      _bfd_evax_output_short (abfd, 0);
      _bfd_evax_output_long (abfd, (unsigned long)(section->index));
      _bfd_evax_output_long (abfd,
			     (unsigned long) bfd_get_start_address (abfd));
      _bfd_evax_output_long (abfd, 0);
    }

  _bfd_evax_output_end (abfd);
  return 0;
}
