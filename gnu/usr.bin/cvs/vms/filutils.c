/*
 * Copyright © 1994 the Free Software Foundation, Inc.
 *
 * Author: Richard Levitte (levitte@e.kth.se)
 *
 * This file is a part of GNU VMSLIB, the GNU library for porting GNU
 * software to VMS.
 *
 * GNU VMSLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNU VMSLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <string.h>
#include <file.h>
#include <rmsdef.h>
#include <fab.h>
#include <nam.h>
#include <stdlib.h>
#include <lib$routines.h>
#include <descrip.h>
#include "filutils.h"

/* file_name_as_directory was snarfed from src/fileio.c in GNU Emacs.  */

char *
file_name_as_directory (out, in)
     char *out, *in;
{
  int size = strlen (in) - 1;
  int ext_point = 0;

  strcpy (out, in);

  /* Is it already a directory string? */
  if (in[size] == ':' || in[size] == ']' || in[size] == '>')
    return out;
  /* Is it a VMS directory file name?  If so, hack VMS syntax.  */
  else
    if (! strchr (in, '/'))
      {
	ext_point = 1;
	if (size > 3 && (! strcmp (&in[size - 3], ".DIR")
			 || ! strcmp (&in[size - 3], ".dir")))
	  ext_point = -3;
	else
	  if (size > 5 && (! strncmp (&in[size - 5], ".DIR", 4)
			   || ! strncmp (&in[size - 5], ".dir", 4))
	      && (in[size - 1] == '.' || in[size - 1] == ';')
	      && in[size] == '1')
	    ext_point = -5;
      }
  if (ext_point != 0)
    {
      register char *p, *dot;
      char brack;

      /* dir:[000000]x.dir --> dir:x.dir --> dir:[x]
	 dir:[000000.x]y.dir --> dir:[x]y.dir --> dir:[x.y]
	 but dir:[000000.000000]x.dir --> dir:[000000.000000.x]
	     dir:[000000.000000.x]y.dir --> dir:[000000.000000.x.y] */
      static char tem[256];

      p = dot = strchr(in,':');
      if (p != 0 && (p[1] == '[' || p[1] == '<'))
	{
	  p += 2;
	  if (strncmp(p,"000000",6) == 0)
	    {
	      p += 6;
	      if (strncmp(p,".000000",7) != 0
		  && (*p == ']' || *p == '>' || *p == '.'))
		{
		  size = dot - in + 1;
		  strncpy(tem, in, size);
		  if (*p == '.')
		    tem[size++] = '[';
		  strcpy(tem + size, p + 1);
		  in = tem;
		  size = strlen(in) - 1;
		}
	    }
	}
      /* x.dir -> [.x]
	 dir:x.dir --> dir:[x]
	 dir:[x]y.dir --> dir:[x.y] */
      p = in + size;
      while (p != in && *p != ':' && *p != '>' && *p != ']') p--;
      {
	char *emergency_dir = 0;
	int emergency_point = 0; /* relative to the end of `out' */

	if (p != in)
	  {
	    strncpy (out, in, p - in);
	    out[p - in] = '\0';
	    if (*p == ':')
	      {
		brack = ']';
		strcat (out, ":[");
		emergency_dir = "000000";
		emergency_point = 0;
	      }
	    else
	      {
		brack = *p;
		strcat (out, ".");
		emergency_dir = "";
		emergency_point = -1;
	      }
	    p++;
	  }
	else
	  {
	    brack = ']';
	    strcpy (out, "[.");
	    emergency_dir = "";
	    emergency_point = -2;
	  }
	if (strncmp (p, "000000.", 7) == 0
	    && (strncmp (p+7, "000000", 6) != 0
		|| (p[13] != ']' && p[13] != '>' && p[13] != '.')))
	  p += 7;
	if (p < (in + size + ext_point))
	  {
	    register copy_len = ((in + size + ext_point) - p);
	    size = strlen (out) + copy_len;
	    strncat (out, p, copy_len);
	  }
	else
	  {
	    size = strlen (out) + emergency_point;
	    strcpy (out + size, emergency_dir);
	    size += strlen (emergency_dir);
	  }
      }
      out[size++] = brack;
      out[size] = '\0';
    }
  return out;
}

/*
 * Convert from directory name to filename.
 * On VMS:
 *       xyzzy:[mukesh.emacs] => xyzzy:[mukesh]emacs.dir.1
 *       xyzzy:[mukesh] => xyzzy:[000000]mukesh.dir.1
 * On UNIX, it's simple: just make sure there is a terminating /

 * Value is nonzero if the string output is different from the input.
 */

/* directory_file_name was snarfed from src/fileio.c in GNU Emacs.  */

#include <stdio.h>
directory_file_name (src, dst)
     char *src, *dst;
{
  long slen;
  long rlen;
  char * ptr, * rptr;
  char bracket;
  struct FAB fab = cc$rms_fab;
  struct NAM nam = cc$rms_nam;
  char esa[NAM$C_MAXRSS];

  slen = strlen (src);

  if (! strchr (src, '/')
      && (src[slen - 1] == ']'
	  || src[slen - 1] == ':'
	  || src[slen - 1] == '>'))
    {
      /* VMS style - convert [x.y.z] to [x.y]z, [x] to [000000]x */
      fab.fab$l_fna = src;
      fab.fab$b_fns = slen;
      fab.fab$l_nam = &nam;
      fab.fab$l_fop = FAB$M_NAM;

      nam.nam$l_esa = esa;
      nam.nam$b_ess = sizeof esa;
      nam.nam$b_nop |= NAM$M_SYNCHK;

      /* We call SYS$PARSE to handle such things as [--] for us. */
      if (SYS$PARSE(&fab, 0, 0) == RMS$_NORMAL)
	{
	  slen = nam.nam$b_esl;
	  if (esa[slen - 1] == ';' && esa[slen - 2] == '.')
	    slen -= 2;
	  esa[slen] = '\0';
	  src = esa;
	}
      if (src[slen - 1] != ']' && src[slen - 1] != '>')
	{
	  /* what about when we have logical_name:???? */
	  if (src[slen - 1] == ':')
	    {			/* Xlate logical name and see what we get */
	      ptr = strcpy (dst, src); /* upper case for getenv */
	      while (*ptr)
		{
		  if ('a' <= *ptr && *ptr <= 'z')
		    *ptr -= 040;
		  ptr++;
		}
	      dst[slen - 1] = 0;	/* remove colon */
	      if (!(src = getenv (dst)))
		return 0;
	      /* should we jump to the beginning of this procedure?
		 Good points: allows us to use logical names that xlate
		 to Unix names,
		 Bad points: can be a problem if we just translated to a device
		 name...
		 For now, I'll punt and always expect VMS names, and hope for
		 the best! */
	      slen = strlen (src);
	      if (src[slen - 1] != ']' && src[slen - 1] != '>')
		{ /* no recursion here! */
		  strcpy (dst, src);
		  return 0;
		}
	    }
	  else
	    {		/* not a directory spec */
	      strcpy (dst, src);
	      return 0;
	    }
	}
      bracket = src[slen - 1];

      /* If bracket is ']' or '>', bracket - 2 is the corresponding
	 opening bracket.  */
      ptr = strchr (src, bracket - 2);
      if (ptr == 0)
	{ /* no opening bracket */
	  strcpy (dst, src);
	  return 0;
	}
      if (!(rptr = strrchr (src, '.')))
	rptr = ptr;
      slen = rptr - src;
      strncpy (dst, src, slen);
      dst[slen] = '\0';
#if 0
      fprintf (stderr, "dst = \"%s\"\nsrc = \"%s\"\nslen = %d\n",
	       dst, src, slen);
#endif
      if (*rptr == '.')
	{
	  dst[slen++] = bracket;
	  dst[slen] = '\0';
	}
      else
	{
	  /* If we have the top-level of a rooted directory (i.e. xx:[000000]),
	     then translate the device and recurse. */
	  if (dst[slen - 1] == ':'
	      && dst[slen - 2] != ':'	/* skip decnet nodes */
	      && ((src[slen] == '['
		   && strcmp(src + slen + 1, "000000]") == 0)
		  || src[slen] == '<'
		  && strcmp(src + slen + 1, "000000>") == 0))
	    {
	      static char equiv_buf[256];
	      static struct dsc$descriptor_s equiv
		= {sizeof (equiv_buf), DSC$K_DTYPE_T, DSC$K_CLASS_S, equiv_buf};
	      static struct dsc$descriptor_s d_name
		= {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
	      short eqlen;

	      dst[slen - 1] = '\0';
	      d_name.dsc$w_length = strlen (dst);
	      d_name.dsc$a_pointer = dst;
	      if (LIB$SYS_TRNLOG (&d_name, &eqlen, &equiv) == 1
		  && (equiv_buf[eqlen] = '\0', ptr = equiv_buf) != 0
		  && (rlen = strlen (ptr) - 1) > 0
		  && (ptr[rlen] == ']' || ptr[rlen] == '>')
		  && ptr[rlen - 1] == '.')
		{
		  char * buf = (char *) malloc (strlen (ptr) + 1);
		  int tmp = ptr[rlen];
		  if (buf == 0)
		    return 0; /* bad luck */
		  strcpy (buf, ptr);
		  buf[rlen - 1] = tmp;
		  buf[rlen] = '\0';
		  tmp = directory_file_name (buf, dst);
		  free (buf);
		  return tmp;
		}
	      else
		dst[slen - 1] = ':';
	    }
	  strcat (dst, "[000000]");
	  slen += 8;
	}
      rptr++;
      rlen = strlen (rptr) - 1;
      strncat (dst, rptr, rlen);
      dst[slen + rlen] = '\0';
      strcat (dst, ".DIR.1");
      return 1;
    }

  /* Process as Unix format: just remove any final slash.
     But leave "/" unchanged; do not change it to "".  */
  strcpy (dst, src);
  if (slen > 1 && dst[slen - 1] == '/')
    {
      dst[slen - 1] = 0;
      return 1;
    }
  return 0;
}

/* end of snarf.  */
