#include "pwd.h"
#include <stdio.h>
#include <unixlib.h>

#ifndef __VMS_VER
#define __VMS_VER 0
#endif
#ifndef __DECC_VER
#define __DECC_VER 0
#endif

#if __VMS_VER < 70200000 || __DECC_VER < 50700000

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

#else  /*  __VMS_VER >= 70200000 && __DECC_VER >= 50700000  */
#pragma message disable EMPTYFILE
#endif  /*  __VMS_VER >= 70200000 && __DECC_VER >= 50700000  */
