#include <stdio.h>
#include <unixio.h>

int rmdir(path)
char *path;
{
  chmod(path, 0777);
  return remove(path);
}

