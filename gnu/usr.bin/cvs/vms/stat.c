#include <string.h>
#include <stat.h>
#include <unixlib.h>

int wrapped_stat (path, buffer)
const char *path;
struct stat *buffer;
{
  char statpath[1024];
  int rs;

  strcpy(statpath, path);
  strip_trailing_slashes (statpath);
  if(strcmp(statpath, ".") == 0)
  {
      char *wd;
      wd = xgetwd ();
      rs = stat (wd, buffer);
      free (wd);
  }
  else
      rs = stat (statpath, buffer);

  if (rs < 0)
      {
      /* If stat() fails try again after appending ".dir" to the filename
         this allows you to stat things like "bloogle/CVS" from VMS 6.1 */
      strcat(statpath, ".dir");
      rs = stat (statpath, buffer);
      }

  return rs;
}
