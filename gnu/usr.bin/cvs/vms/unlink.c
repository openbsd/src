#include <unixio.h>

/* UNIX-like file deletion, deletes previous VMS file versions so UNIX
   style locking through files dosen't lose.  */
int unlink(char *path)
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
