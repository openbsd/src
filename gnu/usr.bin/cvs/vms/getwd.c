#include <unixlib.h>

char *getwd(char *pathname)
{
  return getcwd(pathname, 256, 0);
}
