/**********************************************************************
*
*	C Source:		type.c
*	Instance:		idc_rads_2
*	Description:	DOS type Emulation
*	%created_by:	smscm %
*	%date_created:	Fri Apr 20 19:05:34 2001 %
*
**********************************************************************/
#ifndef lint
static char *_csrc = "@(#) %filespec: type.c~1 %  (%full_filespec: type.c~1:csrc:idc_rads#3 %)";
#endif

#include <stdio.h>
#include <nwfattr.h>
#include "clibstuf.h"

void main (int argc, char** argv)
{
  FILE* pfile = NULL;
  int k;
  int thechar;
  char* defaultDir;

  fnInitGpfGlobals();
  SetCurrentNameSpace(NWOS2_NAME_SPACE);
  defaultDir = (char *)getenv("PERL_ROOT");
  if (!defaultDir || (strlen(defaultDir) == 0))
    defaultDir = "sys:\\perl\\scripts";
  chdir(defaultDir);

  k = 1;
  while (k < argc)
  {
    // open the next file and print it out
    pfile = fopen(argv[k],"r");
    if (pfile)
    {
      while ((thechar = getc(pfile)) != EOF)
      {
        if (thechar != 0x0d)
          printf("%c",thechar);
      }
      fclose (pfile);
    }
    k++;
  }
}
