/* mkdir.c --- mkdir for Windows NT
   Jim Blandy <jimb@cyclic.com> --- July 1995  */

#include <assert.h>

#include "cvs.h"

int
wnt_mkdir (const char *path, int mode)
{
  /* This is true for all extant calls to CVS_MKDIR.  If
     someone adds a call that uses something else later,
     we should tweak this function to handle that.  */
  assert (mode == 0777);

  return mkdir (path);
}
