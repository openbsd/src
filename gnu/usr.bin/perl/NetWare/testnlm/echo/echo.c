/**********************************************************************
*
*	C Source:		echo.c
*	Instance:		idc_rads_2
*	Description:	DOS echo Emulation
*	%created_by:	smscm %
*	%date_created:	Fri Apr 20 19:05:31 2001 %
*
**********************************************************************/
#ifndef lint
static char *_csrc = "@(#) %filespec: echo.c~1 %  (%full_filespec: echo.c~1:csrc:idc_rads#3 %)";
#endif

#include <stdio.h>
//#include <process.h>
#include "clibstuf.h"

void main (int argc, char** argv)
{
  fnInitGpfGlobals();
  if (argc>1 && argv[1]!=NULL && strcmp(argv[1],"-d")==0) {
    int n;
    for (n=0; n < argc; n++) {
      printf("%2d: '%s'\n", n, argv[n]);
    }
  } else {
    while (--argc) {
      printf("%s%c", *++argv, argc==1 ? '\n' : ' ');
    }
  }
}
