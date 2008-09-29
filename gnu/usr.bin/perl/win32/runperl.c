#include "EXTERN.h"
#include "perl.h"

#ifdef __GNUC__

/* Mingw32 defaults to globing command line 
 * This is inconsistent with other Win32 ports and 
 * seems to cause trouble with passing -DXSVERSION=\"1.6\" 
 * So we turn it off like this, but only when compiling
 * perlmain.c: perlmainst.c is linked into the same executable
 * as win32.c, which also does this, so we mustn't do it twice
 * otherwise we get a multiple definition error.
 */
#ifndef PERLDLL
int _CRT_glob = 0;
#endif

#endif

int
main(int argc, char **argv, char **env)
{
    return RunPerl(argc, argv, env);
}


