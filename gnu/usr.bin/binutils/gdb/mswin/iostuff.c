#ifndef APIENTRY
#define APIENTRY
#endif
#define W32SUT_16

#include "windefs.h"
#include "./w32sut.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#undef WINDOWS
#include "c:/msvc/include/bios.h"
#define WINDOWS


int FAR PASCAL LibMain (HANDLE hLibInst, WORD wDataSeg,
			WORD cbHeapSize, LPSTR lpszCmdLine)
{
  return (1);
} // LibMain()

DWORD FAR PASCAL __export UTInit( UT16CBPROC lpfnUT16CallBack,LPVOID lpBuf )
{
  return(1); // Return Success
} // UTInit()



/*
 * Call the appropriate Win16 API based on the dwFunc parameter.  Extract any
 * necessary parameters from the lpBuf buffer.
 *
 * This function must be exported.
 *
 */


DWORD FAR PASCAL __export UTProc( DWORD *lpArgs, DWORD dwFunc)
{

  switch (dwFunc)
    {
    case 0:
      _bios_serialcom(_COM_INIT, 0, _COM_9600|_COM_NOPARITY|_COM_CHR8|_COM_STOP1);
    case 1:
      return _bios_serialcom(_COM_STATUS, 0,0);
    case 2:
      return _bios_serialcom(_COM_RECEIVE, 0,0);
    case 3:
      {
	int c = lpArgs[0];
	return _bios_serialcom(_COM_SEND, 0, lpArgs[0]);
      }
    }

  return( (DWORD)-1L ); // We should never get here.

} // UTProc()
