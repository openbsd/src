/* valloc -- return memory aligned to the page size.  */

#ifndef HAVE_GETPAGESIZE
#define getpagesize() 4096
#endif

extern char *malloc ();

char *
valloc (bytes)
     int bytes;
{
  long pagesize;
  char *ret;

  pagesize = getpagesize ();
  ret = (char *) malloc (bytes + pagesize - 1);
  if (ret)
    ret = (char *) ((long) (ret + pagesize - 1) &~ (pagesize - 1));
  return ret;
}
