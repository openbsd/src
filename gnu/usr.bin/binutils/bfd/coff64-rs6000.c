/* BFD back-end for IBM RS/6000 "XCOFF64" files.
   Copyright 2000, 2001
   Free Software Foundation, Inc.
   Written Clinton Popetz.
   Contributed by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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

/* Internalcoff.h and coffcode.h modify themselves based on these flags.  */
#define XCOFF64
#define RS6000COFF_C 1

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "coff/rs6k64.h"
#include "libcoff.h"

#define GET_FILEHDR_SYMPTR bfd_h_get_64
#define PUT_FILEHDR_SYMPTR bfd_h_put_64
#define GET_AOUTHDR_DATA_START bfd_h_get_64
#define PUT_AOUTHDR_DATA_START bfd_h_put_64
#define GET_AOUTHDR_TEXT_START bfd_h_get_64
#define PUT_AOUTHDR_TEXT_START bfd_h_put_64
#define GET_AOUTHDR_TSIZE bfd_h_get_64
#define PUT_AOUTHDR_TSIZE bfd_h_put_64
#define GET_AOUTHDR_DSIZE bfd_h_get_64
#define PUT_AOUTHDR_DSIZE bfd_h_put_64
#define GET_AOUTHDR_BSIZE bfd_h_get_64
#define PUT_AOUTHDR_BSIZE bfd_h_put_64
#define GET_AOUTHDR_ENTRY bfd_h_get_64
#define PUT_AOUTHDR_ENTRY bfd_h_put_64
#define GET_SCNHDR_PADDR bfd_h_get_64
#define PUT_SCNHDR_PADDR bfd_h_put_64
#define GET_SCNHDR_VADDR bfd_h_get_64
#define PUT_SCNHDR_VADDR bfd_h_put_64
#define GET_SCNHDR_SIZE bfd_h_get_64
#define PUT_SCNHDR_SIZE bfd_h_put_64
#define GET_SCNHDR_SCNPTR bfd_h_get_64
#define PUT_SCNHDR_SCNPTR bfd_h_put_64
#define GET_SCNHDR_RELPTR bfd_h_get_64
#define PUT_SCNHDR_RELPTR bfd_h_put_64
#define GET_SCNHDR_LNNOPTR bfd_h_get_64
#define PUT_SCNHDR_LNNOPTR bfd_h_put_64
#define GET_SCNHDR_NRELOC bfd_h_get_32
#define MAX_SCNHDR_NRELOC 0xffffffff
#define PUT_SCNHDR_NRELOC bfd_h_put_32
#define GET_SCNHDR_NLNNO bfd_h_get_32
#define MAX_SCNHDR_NLNNO 0xffffffff
#define PUT_SCNHDR_NLNNO bfd_h_put_32
#define GET_RELOC_VADDR bfd_h_get_64
#define PUT_RELOC_VADDR bfd_h_put_64

#define COFF_FORCE_SYMBOLS_IN_STRINGS
#define COFF_DEBUG_STRING_WIDE_PREFIX

#define COFF_ADJUST_SCNHDR_OUT_POST(ABFD,INT,EXT) \
do { \
  memset (((SCNHDR *)EXT)->s_pad, 0, sizeof (((SCNHDR *)EXT)->s_pad));\
} while (0)

#define NO_COFF_LINENOS

#define coff_SWAP_lineno_in xcoff64_swap_lineno_in
#define coff_SWAP_lineno_out xcoff64_swap_lineno_out

#define PUTWORD bfd_h_put_32
#define PUTHALF bfd_h_put_16
#define PUTBYTE bfd_h_put_8
#define GETWORD bfd_h_get_32
#define GETHALF bfd_h_get_16
#define GETBYTE bfd_h_get_8

/* For XCOFF64, the effective width of symndx changes depending on
   whether we are the first entry.  Sigh.  */
static void
xcoff64_swap_lineno_in (abfd, ext1, in1)
     bfd            *abfd;
     PTR ext1;
     PTR in1;
{
  LINENO *ext = (LINENO *)ext1;
  struct internal_lineno      *in = (struct internal_lineno *)in1;

  in->l_lnno = bfd_h_get_32(abfd, (bfd_byte *) (ext->l_lnno));
  if (in->l_lnno == 0)
    in->l_addr.l_symndx =
	    bfd_h_get_32(abfd, (bfd_byte *) ext->l_addr.l_symndx);
  else
    in->l_addr.l_symndx =
	    bfd_h_get_64(abfd, (bfd_byte *) ext->l_addr.l_symndx);
}

static unsigned int
xcoff64_swap_lineno_out (abfd, inp, outp)
     bfd       *abfd;
     PTR	inp;
     PTR	outp;
{
  struct internal_lineno *in = (struct internal_lineno *)inp;
  struct external_lineno *ext = (struct external_lineno *)outp;
  PUTWORD(abfd, in->l_addr.l_symndx, (bfd_byte *)
	  ext->l_addr.l_symndx);

  bfd_h_put_32 (abfd, in->l_lnno,  (bfd_byte *) (ext->l_lnno));
  if (in->l_lnno == 0)
    bfd_h_put_32 (abfd, in->l_addr.l_symndx, (bfd_byte *)ext->l_addr.l_symndx);
  else
    bfd_h_put_64 (abfd, in->l_addr.l_symndx, (bfd_byte *)ext->l_addr.l_symndx);

  return bfd_coff_linesz (abfd);
}

static void xcoff64_swap_sym_in PARAMS ((bfd *, PTR, PTR));
static unsigned int xcoff64_swap_sym_out PARAMS ((bfd *, PTR, PTR));
static void xcoff64_swap_aux_in PARAMS ((bfd *, PTR, int, int, int, int, PTR));
static unsigned int xcoff64_swap_aux_out PARAMS ((bfd *, PTR, int, int, int, int, PTR));

static void
xcoff64_swap_sym_in (abfd, ext1, in1)
     bfd            *abfd;
     PTR ext1;
     PTR in1;
{
  SYMENT *ext = (SYMENT *)ext1;
  struct internal_syment      *in = (struct internal_syment *)in1;

  in->_n._n_n._n_zeroes = 0;
  in->_n._n_n._n_offset = bfd_h_get_32(abfd, (bfd_byte *) ext->e_offset);
  in->n_value = bfd_h_get_64(abfd, (bfd_byte *) ext->e.e_value);
  in->n_scnum = bfd_h_get_16(abfd, (bfd_byte *) ext->e_scnum);
  in->n_type = bfd_h_get_16(abfd, (bfd_byte *) ext->e_type);
  in->n_sclass = bfd_h_get_8(abfd, ext->e_sclass);
  in->n_numaux = bfd_h_get_8(abfd, ext->e_numaux);
}

static unsigned int
xcoff64_swap_sym_out (abfd, inp, extp)
     bfd       *abfd;
     PTR	inp;
     PTR	extp;
{
  struct internal_syment *in = (struct internal_syment *)inp;
  SYMENT *ext =(SYMENT *)extp;

  bfd_h_put_32(abfd, in->_n._n_n._n_offset, (bfd_byte *)  ext->e_offset);
  bfd_h_put_64(abfd,  in->n_value , (bfd_byte *) ext->e.e_value);
  bfd_h_put_16(abfd,  in->n_scnum , (bfd_byte *) ext->e_scnum);
  bfd_h_put_16(abfd,  in->n_type , (bfd_byte *) ext->e_type);
  bfd_h_put_8(abfd,  in->n_sclass , ext->e_sclass);
  bfd_h_put_8(abfd,  in->n_numaux , ext->e_numaux);
  return bfd_coff_symesz (abfd);
}

static void
xcoff64_swap_aux_in (abfd, ext1, type, class, indx, numaux, in1)
     bfd            *abfd;
     PTR 	      ext1;
     int             type;
     int             class;
     int	      indx;
     int	      numaux;
     PTR 	      in1;
{
  AUXENT    *ext = (AUXENT *)ext1;
  union internal_auxent *in = (union internal_auxent *)in1;

  switch (class) {
    case C_FILE:
      if (ext->x_file.x_fname[0] == 0) {
	  in->x_file.x_n.x_zeroes = 0;
	  in->x_file.x_n.x_offset =
	   bfd_h_get_32(abfd, (bfd_byte *) ext->x_file.x_n.x_offset);
	} else {
	    if (numaux > 1)
	      {
		if (indx == 0)
	  memcpy (in->x_file.x_fname, ext->x_file.x_fname,
			  numaux * sizeof (AUXENT));
	      }
	    else
	      {
		memcpy (in->x_file.x_fname, ext->x_file.x_fname, FILNMLEN);
	      }
	  }
      goto end;

      /* RS/6000 "csect" auxents */
    case C_EXT:
    case C_HIDEXT:
      if (indx + 1 == numaux)
	{
	  in->x_csect.x_scnlen.l =
	      bfd_h_get_32(abfd, ext->x_csect.x_scnlen_lo);
	  /* FIXME: If we want section lengths larger than 32 bits, we need
	     to modify the internal coff structures to support it.  */
	  in->x_csect.x_parmhash = bfd_h_get_32 (abfd,
						 ext->x_csect.x_parmhash);
	  in->x_csect.x_snhash   = bfd_h_get_16 (abfd, ext->x_csect.x_snhash);
	  /* We don't have to hack bitfields in x_smtyp because it's
	     defined by shifts-and-ands, which are equivalent on all
	     byte orders.  */
	  in->x_csect.x_smtyp    = bfd_h_get_8  (abfd, ext->x_csect.x_smtyp);
	  in->x_csect.x_smclas   = bfd_h_get_8  (abfd, ext->x_csect.x_smclas);
	  goto end;
	}
      break;

    case C_STAT:
    case C_LEAFSTAT:
    case C_HIDDEN:
      if (type == T_NULL) {
	  /* PE defines some extra fields; we zero them out for
             safety.  */
	  in->x_scn.x_checksum = 0;
	  in->x_scn.x_associated = 0;
	  in->x_scn.x_comdat = 0;

	  goto end;
	}
      break;
    }

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      in->x_sym.x_fcnary.x_fcn.x_lnnoptr = bfd_h_get_64(abfd, (bfd_byte *)
	      ext->x_sym.x_fcnary.x_fcn.x_lnnoptr);
      in->x_sym.x_fcnary.x_fcn.x_endndx.l = bfd_h_get_32(abfd, (bfd_byte *)
	      ext->x_sym.x_fcnary.x_fcn.x_endndx);
    }
  if (ISFCN(type)) {
    in->x_sym.x_misc.x_fsize = bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_fsize);
  }
  else {
    in->x_sym.x_misc.x_lnsz.x_lnno = bfd_h_get_32(abfd, (bfd_byte *)
	    ext->x_sym.x_fcnary.x_lnsz.x_lnno);
    in->x_sym.x_misc.x_lnsz.x_size = bfd_h_get_16(abfd, (bfd_byte *)
	    ext->x_sym.x_fcnary.x_lnsz.x_size);
  }

end: ;
  /* the semicolon is because MSVC doesn't like labels at
     end of block.  */

}

static unsigned int
xcoff64_swap_aux_out (abfd, inp, type, class, indx, numaux, extp)
     bfd   *abfd;
     PTR 	inp;
     int   type;
     int   class;
     int   indx ATTRIBUTE_UNUSED;
     int   numaux ATTRIBUTE_UNUSED;
     PTR	extp;
{
  union internal_auxent *in = (union internal_auxent *)inp;
  AUXENT *ext = (AUXENT *)extp;

  memset((PTR)ext, 0, bfd_coff_auxesz (abfd));
  switch (class)
    {
  case C_FILE:
    if (in->x_file.x_fname[0] == 0)
      {
      PUTWORD(abfd, 0, (bfd_byte *) ext->x_file.x_n.x_zeroes);
      PUTWORD(abfd,
	      in->x_file.x_n.x_offset,
	      (bfd_byte *) ext->x_file.x_n.x_offset);
    }
    else
      {
      memcpy (ext->x_file.x_fname, in->x_file.x_fname, FILNMLEN);
      }
    PUTBYTE (abfd, _AUX_FILE, (bfd_byte *) ext->x_auxtype.x_auxtype);
    goto end;

  /* RS/6000 "csect" auxents */
  case C_EXT:
  case C_HIDEXT:
    if (indx + 1 == numaux)
      {
	PUTWORD (abfd, in->x_csect.x_scnlen.l, ext->x_csect.x_scnlen_lo);
	PUTWORD (abfd, in->x_csect.x_parmhash,	ext->x_csect.x_parmhash);
	PUTHALF (abfd, in->x_csect.x_snhash,	ext->x_csect.x_snhash);
	/* We don't have to hack bitfields in x_smtyp because it's
	   defined by shifts-and-ands, which are equivalent on all
	   byte orders.  */
	PUTBYTE (abfd, in->x_csect.x_smtyp,	ext->x_csect.x_smtyp);
	PUTBYTE (abfd, in->x_csect.x_smclas,	ext->x_csect.x_smclas);
    	PUTBYTE (abfd, _AUX_CSECT, (bfd_byte *) ext->x_auxtype.x_auxtype);
	goto end;
      }
    break;

  case C_STAT:
  case C_LEAFSTAT:
  case C_HIDDEN:
    if (type == T_NULL) {
      goto end;
    }
    break;
  }

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      bfd_h_put_64(abfd,  in->x_sym.x_fcnary.x_fcn.x_lnnoptr,
	      (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_lnnoptr);
      PUTBYTE (abfd, _AUX_FCN, (bfd_byte *) ext->x_auxtype.x_auxtype);
      PUTWORD(abfd,  in->x_sym.x_fcnary.x_fcn.x_endndx.l,
	      (bfd_byte *) ext->x_sym.x_fcnary.x_fcn.x_endndx);
    }
  if (ISFCN (type))
    PUTWORD (abfd, in->x_sym.x_misc.x_fsize,
	     (bfd_byte *)  ext->x_sym.x_fcnary.x_fcn.x_fsize);
  else
    {
      bfd_h_put_32(abfd, in->x_sym.x_misc.x_lnsz.x_lnno,
	      (bfd_byte *)ext->x_sym.x_fcnary.x_lnsz.x_lnno);
      bfd_h_put_16(abfd, in->x_sym.x_misc.x_lnsz.x_size,
	      (bfd_byte *)ext->x_sym.x_fcnary.x_lnsz.x_size);
    }

end:
  return bfd_coff_auxesz (abfd);
}

#define coff_SWAP_sym_in xcoff64_swap_sym_in
#define coff_SWAP_sym_out xcoff64_swap_sym_out
#define coff_SWAP_aux_in xcoff64_swap_aux_in
#define coff_SWAP_aux_out xcoff64_swap_aux_out

#define TARGET_NAME "aixcoff64-rs6000"
#define TARGET_SYM rs6000coff64_vec

#include "xcoff-target.h"
