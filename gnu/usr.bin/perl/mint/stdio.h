/* Wrapper around broken system stdio.h.  */

#ifndef _PERL_WRAPPER_AROUND_STDIO_H
# define _PERL_WRAPPER_AROUND_STDIO_H 1

/* The MiNTLib has a macro called EOS in stdio.h.  This conflicts
   with regnode.h.  Who had this glorious idea.  */
#ifdef EOS
# define PERL_EOS EOS
#endif

/* First include the system file.  */
#include_next <stdio.h> 

#ifdef EOS
# undef EOS
# define EOS PERL_EOS
#endif

#endif

