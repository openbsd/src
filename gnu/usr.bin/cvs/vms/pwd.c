#include "pwd.h"
#include <stdio.h>
#include <unixlib.h>

static struct passwd pw;

/* This is only called from one relevant place, lock.c.  In that context
   the code is really trying to figure out who owns a directory.  Nothing
   which has anything to do with getpwuid or anything of the sort can help
   us on VMS (getuid returns only the group part of the UIC).  */
struct passwd *getpwuid(unsigned int uid)
{
    return NULL;
}

char *getlogin()
{
  static char login[256];
  return cuserid(login);
} 
