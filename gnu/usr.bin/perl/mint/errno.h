/* Wrapper around broken system errno.h.  */

#ifndef _PERL_WRAPPER_AROUND_ERRNO_H
# define _PERL_WRAPPER_AROUND_ERRNO_H 1

/* First include the system file.  */
#include_next <errno.h> 

/* Now add the missing stuff.
#ifndef EAGAIN
# define EAGAIN EWOULDBLOCK
#endif

/* This one is problematic.  If you open() a directory with the
   MiNTLib you can't detect from errno if it is really a directory
   or if the file simply doesn't exist.  You'll get ENOENT 
   ("file not found") in either case.
   
   Defining EISDIR as ENOENT is actually a bad idea but works fine
   in general.  In praxi, if code checks for errno == EISDIR it
   will attempt an opendir() call on the file in question and this
   call will also file if the file really can't be found.  But
   you may get compile-time errors if the errno checking is embedded
   in a switch statement ("duplicate case value in switch").
   
   Anyway, here the define works alright.  */
#ifndef EISDIR
# define EISDIR ENOENT
#endif

#endif

