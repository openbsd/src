
#ifndef __VMS_VER
#define __VMS_VER 0
#endif
#ifndef __DECC_VER
#define __DECC_VER 0
#endif

#if __VMS_VER < 70200000 || __DECC_VER < 50700000

#include <stdio.h>
#include <unixio.h>

int rmdir(path)
char *path;
{
  chmod(path, 0777);
  return remove(path);
}

#else  /*  __VMS_VER >= 70200000 && __DECC_VER >= 50700000  */
#pragma message disable EMPTYFILE
#endif  /*  __VMS_VER >= 70200000 && __DECC_VER >= 50700000  */
