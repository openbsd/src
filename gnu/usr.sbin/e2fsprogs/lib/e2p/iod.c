/*
 * iod.c		- Iterate a function on each entry of a directory
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#include "e2p.h"

int iterate_on_dir (const char * dir_name,
		    int (*func) (const char *, struct dirent *, void *),
		    void * private)
{
	DIR * dir;
#if HAVE_DIRENT_NAMELEN
	/* Declare DE_BUF with some extra room for the name.  */
	char de_buf[sizeof (struct dirent) + 32];
	struct dirent *de = (struct dirent *)&de_buf;
#else
	struct dirent de_buf, *de = &de_buf;
#endif
	struct dirent *dep;

	dir = opendir (dir_name);
	if (dir == NULL)
		return -1;
	while ((dep = readdir (dir)))
	{
#if HAVE_DIRENT_NAMELEN
	  /* See if there's enough room for this entry in DE, and grow if
	     not.  */
	  if (de_len < dep->d_reclen)
	    {
	      de_len = dep->d_reclen + 32;
	      de =
		(de == (struct dirent *)&de_buf
		 ? malloc (de_len)
		 : realloc (de, de_len));
	      if (de == NULL)
		{
		  errno = ENOMEM;
		  return -1;
		}
	    }
	  memcpy (de, dep, dep->d_reclen);
#else
	  *de = *dep;
#endif
	  (*func) (dir_name, de, private);
	}
#if HAVE_DIRENT_NAMELEN
	if (de != (struct dirent *)&de_buf)
	  free (de);
#endif
	closedir (dir);
	return 0;
}
