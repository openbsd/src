#include <stdio.h>

int remove(const char *filename)
{
  return unlink(filename);
}
