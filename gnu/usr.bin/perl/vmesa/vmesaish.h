#ifndef _VMESA_INCLUDED
# define _VMESA_INCLUDED 1
# include <string.h>
# include <ctype.h>
# include <vmsock.h>
 void * dlopen(const char *);
 void * dlsym(void *, const char *);
 void * dlerror(void);
# define OLD_PTHREADS_API
#endif
