/*
**	$Id: config.c,v 1.2 1997/07/23 20:36:26 kstailey Exp $
**
** config.c			    This file handles the config file
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 6 Dec 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#include <stdio.h>
#include <errno.h>

#include "error.h"
#include "identd.h"
#include "paths.h"


int parse_config(path, silent_flag)
  char *path;
  int silent_flag;
{
  FILE *fp;

  if (!path)
    path = PATH_CONFIG;
  
  fp = fopen(path, "r");
  if (!fp)
  {
    if (silent_flag)
      return 0;

    ERROR1("error opening %s", path);
  }

  /*
  ** Code should go here to parse the config file data.
  ** For now we just ignore the contents...
  */
  
  
  fclose(fp);
  return 0;
}
