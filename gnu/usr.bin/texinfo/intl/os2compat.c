/* OS/2 compatibility functions.
   Copyright (C) 2001-2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#define OS2_AWARE
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

/* A version of getenv() that works from DLLs */
extern unsigned long DosScanEnv (const unsigned char *pszName, unsigned char **ppszValue);

char *
_nl_getenv (const char *name)
{
  unsigned char *value;
  if (DosScanEnv (name, &value))
    return NULL;
  else
    return value;
}

/* A fixed size buffer.  */
#define LOCALEDIR_MAX 260
char _nl_default_dirname__[LOCALEDIR_MAX+1];

char *_os2_libdir = NULL;
char *_os2_localealiaspath = NULL;
char *_os2_localedir = NULL;

static __attribute__((constructor)) void
os2_initialize ()
{
  char *root = getenv ("UNIXROOT");
  char *gnulocaledir = getenv ("GNULOCALEDIR");

  _os2_libdir = gnulocaledir;
  if (!_os2_libdir)
    {
      if (root)
        {
          size_t sl = strlen (root);
          _os2_libdir = (char *) malloc (sl + strlen (LIBDIR) + 1);
          memcpy (_os2_libdir, root, sl);
          memcpy (_os2_libdir + sl, LIBDIR, strlen (LIBDIR) + 1);
        }
      else
        _os2_libdir = LIBDIR;
    }

  _os2_localealiaspath = gnulocaledir;
  if (!_os2_localealiaspath)
    {
      if (root)
        {
          size_t sl = strlen (root);
          _os2_localealiaspath = (char *) malloc (sl + strlen (LOCALE_ALIAS_PATH) + 1);
          memcpy (_os2_localealiaspath, root, sl);
          memcpy (_os2_localealiaspath + sl, LOCALE_ALIAS_PATH, strlen (LOCALE_ALIAS_PATH) + 1);
        }
     else
        _os2_localealiaspath = LOCALE_ALIAS_PATH;
    }

  _os2_localedir = gnulocaledir;
  if (!_os2_localedir)
    {
      if (root)
        {
          size_t sl = strlen (root);
          _os2_localedir = (char *) malloc (sl + strlen (LOCALEDIR) + 1);
          memcpy (_os2_localedir, root, sl);
          memcpy (_os2_localedir + sl, LOCALEDIR, strlen (LOCALEDIR) + 1);
        }
      else
        _os2_localedir = LOCALEDIR;
    }

  {
    extern const char _nl_default_dirname__[];
    if (strlen (_os2_localedir) <= LOCALEDIR_MAX)
      strcpy (_nl_default_dirname__, _os2_localedir);
  }
}
