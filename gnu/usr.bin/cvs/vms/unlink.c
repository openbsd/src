#include <unixio.h>

/* UNIX-like file deletion, deletes previous VMS file versions so UNIX
   style locking through files dosen't lose.  */
#ifndef __VMS_VER
int vms_unlink(char *path)
#elif __VMS_VER < 70200000
int vms_unlink(char *path)
#else
int vms_unlink(char const*path)
#endif
{
  int rs, junk_rs;

  rs = remove(path);
  while(remove(path) >= 0);

  return rs;
}

int link(char *from, char *to)
{
  int rs = -1;

  /* Link always fails */

  return rs;
}
