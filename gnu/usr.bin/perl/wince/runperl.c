#include "EXTERN.h"
#include "perl.h"

#ifdef __GNUC__

/* Mingw32 defaults to globing command line 
 * This is inconsistent with other Win32 ports and 
 * seems to cause trouble with passing -DXSVERSION=\"1.6\" 
 * So we turn it off like this:
 */
int _CRT_glob = 0;

#endif

int
main(int argc, char **argv, char **env)
{
    return RunPerl(argc, argv, env);
}


