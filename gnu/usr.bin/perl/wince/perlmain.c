/* Time-stamp: <01/08/01 20:58:19 keuchel@w2k> */

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

/* Called from w32console/wmain.c */

extern int w32console_usefunctionkeys;

int
main(int argc, char **argv, char **env)
{
  int res;

  if(argc == 1)
    XCEShowMessageA("Starting perl with no args is currently\r\n"
		    "not useful on Windows CE");

  w32console_usefunctionkeys = 0; /* this allows backspace key to work */

  res = RunPerl(argc, argv, env);

  if(res != 0)
    XCEShowMessageA("Exitcode: %d", res);

  return res;
}


