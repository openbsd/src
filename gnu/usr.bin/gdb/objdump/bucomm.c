/* bucomm.c -- Bin Utils COMmon code.
   Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of GNU Binutils.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*   We might put this in a library someday so it could be dynamically
     loaded, but for now it's not necessary */

#include "bfd.h"
#include "sysdep.h"
#include <varargs.h>

char *target = NULL;		/* default as late as possible */

/* Yes, this is what atexit is for, but that isn't guaranteed yet.
   And yes, I know this isn't as good, but it does what is needed just fine */
void (*exit_handler) ();




/* Error reporting */

char *program_name;

void
DEFUN(bfd_fatal,(string),
     char *string)
{
  CONST char *errmsg =  bfd_errmsg (bfd_error);
  
  if (string)
    fprintf (stderr, "%s: %s: %s\n", program_name, string, errmsg);
  else
    fprintf (stderr, "%s: %s\n", program_name, errmsg);

  if (NULL != exit_handler) (*exit_handler) ();
  exit (1);
}

#if 0 /* !defined(NO_STDARG) */
void
fatal (Format)
     const char *Format;
{
  va_list args;
	
  va_start (args, Format);
  vfprintf (stderr, Format, args);
  va_end (args);
  (void) putc ('\n', stderr);
  if (NULL != exit_handler) (*exit_handler) ();
  exit (1);
}
#else
void fatal (va_alist)
     va_dcl
{
	char *Format;
	va_list args;
	
	va_start (args);
	Format = va_arg(args, char *);
	vfprintf (stderr, Format, args);
	va_end (args);
	(void) putc ('\n', stderr);
	if (NULL != exit_handler) (*exit_handler) ();
	exit (1);
} /* fatal() */
#endif


/** Display the archive header for an element as if it were an ls -l listing */

/* Mode       User\tGroup\tSize\tDate               Name */

void
DEFUN(print_arelt_descr,(file, abfd, verbose),
      FILE *file AND
      bfd *abfd AND
      boolean verbose)
{
  void mode_string ();
  struct stat buf;

  if (verbose) {

    if (bfd_stat_arch_elt (abfd, &buf) == 0) { /* if not, huh? */
      char modebuf[11];
      char timebuf[40];
      long when = buf.st_mtime;
      CONST char *ctime_result = (CONST char *)ctime (&when);

      /* Posix format:  skip weekday and seconds from ctime output. */
      sprintf(timebuf, "%.12s %.4s", ctime_result+4, ctime_result+20);

      mode_string (buf.st_mode, modebuf);
      modebuf[10] = '\0';
      /* Posix 1003.2/D11 says to skip first character (entry type). */
      fprintf (file, "%s %d/%d %6qd %s ", modebuf+1, buf.st_uid, buf.st_gid, buf.st_size, timebuf);
    }
  }

  fprintf (file, "%s\n",abfd->filename);
}

/* Like malloc but get fatal error if memory is exhausted.  */
char *
xmalloc (size)
     unsigned size;
{
  register char *result = malloc (size);
  if (result == (char *) NULL && size != 0) {
    fatal ("virtual memory exhausted");
  }

  return result;
}

/* Like realloc but get fatal error if memory is exhausted.  */
char *
xrealloc (ptr, size)
     char *ptr;
     unsigned size;
{
  register char *result = realloc (ptr, size);
  if (result == 0 && size != 0) {
    fatal ("virtual memory exhausted");
  }

  return result;
}
