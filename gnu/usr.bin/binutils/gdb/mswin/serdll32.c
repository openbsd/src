#define W32SUT_32

#include "windefs.h"
#include "w32sut.h"
#include "serdll32.h"
#include "serdll.h"

typedef BOOL (APIENTRY * PUTREGISTER) (HANDLE hModule,
				       LPCSTR lpsz16BitDLL,
				       LPCSTR lpszInitName,
				       LPCSTR lpszProcName,
				       UT32PROC * ppfn32Thunk,
				       FARPROC pfnUT32Callback,
				       LPVOID lpBuff
);


typedef VOID (APIENTRY * PUTUNREGISTER) (HANDLE hModule);

UT32PROC pfnUTProc = NULL;
PUTREGISTER pUTRegister = NULL;
PUTUNREGISTER pUTUnRegister = NULL;
int cProcessesAttached = 0;
BOOL fWin32s = FALSE;
HANDLE hKernel32 = 0;


BOOL APIENTRY 
DllInit (HANDLE hInst, DWORD fdwReason, LPVOID lpReserved)
{
  if (fdwReason == DLL_PROCESS_ATTACH)
    {
      if (cProcessesAttached++)
	{
	  return (TRUE);
	}

      fWin32s = (BOOL) (GetVersion () & 0x80000000);

      if (!fWin32s)
	return (TRUE);

      hKernel32 = LoadLibrary ("Kernel32.Dll");

      pUTRegister = (PUTREGISTER) GetProcAddress (hKernel32, "UTRegister");

      if (!pUTRegister)
	return (FALSE);

      pUTUnRegister = (PUTUNREGISTER) GetProcAddress (hKernel32, "UTUnRegister");

      if (!pUTUnRegister)
	return (FALSE);

      return (*pUTRegister)
	(hInst,
	 "serdll16.DLL",
	 NULL,			// no init routine
	  "UTProc",		// name of 16bit dispatch routine
	  &pfnUTProc,		// Global variable to receive thunk address
	  NULL,			// no callback function
	  NULL);		// no shared memroy

    }
  else if ((fdwReason == DLL_PROCESS_DETACH)
	   && (0 == --cProcessesAttached)
	   && fWin32s)
    {
      (*pUTUnRegister) (hInst);
      FreeLibrary (hKernel32);
    }

}

int APIENTRY 
BuildCommDCB16 (LPCSTR lpszDef, DCB16 * lpdcb)
{

  DWORD Args[2];
  PVOID TransList[3];

  Args[0] = (DWORD) lpszDef;
  Args[1] = (DWORD) lpdcb;

  TransList[0] = &Args[0];
  TransList[1] = &Args[1];
  TransList[2] = NULL;


  return ((*pfnUTProc) (Args, BUILDCOMMDCB, TransList));
}

int APIENTRY 
OpenComm16 (LPCSTR lpszDevControl,
	    USHORT cbInQueue,
	    USHORT cbOutQueue,
	    LPCSTR dcb)
{
  DWORD Args[4];
  PVOID TransList[3];

  Args[0] = (DWORD) lpszDevControl;
  Args[1] = (DWORD) cbInQueue;
  Args[2] = (DWORD) cbOutQueue;
  Args[3] = (DWORD) dcb;

  TransList[0] = &Args[0];
  TransList[1] = &Args[3];
  TransList[2] = NULL;


  return ((*pfnUTProc) (Args, OPENCOMM, TransList));
}

int APIENTRY 
CloseComm16 (int idComDev)
{
  DWORD Args[1];

  Args[0] = idComDev;

  return ((*pfnUTProc) (Args, CLOSECOMM, NULL));
}

int APIENTRY 
ReadComm16 (INT idComDev, LPVOID lpBuf, INT cbRead)
{
  DWORD Args[3];
  PVOID TransList[2];

  Args[0] = (DWORD) idComDev;
  Args[1] = (DWORD) lpBuf;
  Args[2] = (DWORD) cbRead;

  TransList[0] = &Args[1];
  TransList[1] = NULL;


  return ((*pfnUTProc) (Args, READCOMM, TransList));
}


int APIENTRY 
WriteComm16 (INT idComDev, LPCSTR lpBuf, INT cbWrite)
{
  DWORD Args[3];
  PVOID TransList[2];

  Args[0] = (DWORD) idComDev;
  Args[1] = (DWORD) lpBuf;
  Args[2] = (DWORD) cbWrite;

  TransList[0] = &Args[1];
  TransList[1] = NULL;


  return ((*pfnUTProc) (Args, WRITECOMM, TransList));
}


int APIENTRY 
FlushComm16 (INT idComDev, INT fnQueue)
{
  DWORD Args[2];

  Args[0] = (DWORD) idComDev;
  Args[1] = (DWORD) fnQueue;

  return ((*pfnUTProc) (Args, FLUSHCOMM, NULL));
}

int APIENTRY 
TransmitCommChar16 (INT idComDev, char chTransmit)
{
  DWORD Args[2];

  Args[0] = (DWORD) idComDev;
  Args[1] = (DWORD) chTransmit;

  return ((*pfnUTProc) (Args, TRANSMITCOMMCHAR, NULL));
}

int APIENTRY 
SetCommState16 (DCB16 * lpdcb)
{
  DWORD Args[1];

  PVOID TransList[2];

  Args[0] = (DWORD) lpdcb;

  TransList[0] = &Args[0];
  TransList[1] = NULL;

  return ((*pfnUTProc) (Args, SETCOMMSTATE, TransList));
}

int APIENTRY 
GetCommState16 (INT idComDev, DCB16 * lpdcb)
{
  DWORD Args[1];

  PVOID TransList[2];

  Args[0] = (DWORD) lpdcb;

  TransList[0] = &Args[0];
  TransList[1] = NULL;

  return ((*pfnUTProc) (Args, GETCOMMSTATE, TransList));
}
int APIENTRY 
GetCommReady16 (INT idComDev, char *ptr)
{
  DWORD Args[2];

  PVOID tlist[2];

  Args[0] = (DWORD) idComDev;
  Args[1] = (DWORD) ptr;
  tlist[0] = &Args[1];
  tlist[1] = 0;



  return ((*pfnUTProc) (Args, GETCOMMREADY, tlist));
}
int APIENTRY 
GetCommError16 (INT idComDev, COMSTAT16 * lpComStat)
{
  DWORD Args[2];

  PVOID TransList[2];

  Args[0] = (DWORD) idComDev;
  Args[1] = (DWORD) lpComStat;

  TransList[0] = &Args[1];
  TransList[1] = NULL;

  return ((*pfnUTProc) (Args, GETCOMMERROR, TransList));
}

int APIENTRY 
SetCommBreak16 (INT idComDev)
{
  DWORD Args[1];

  Args[0] = (DWORD) idComDev;

  return ((*pfnUTProc) (Args, SETCOMMBREAK, NULL));
}


int APIENTRY 
ClearCommBreak16 (INT idComDev)
{
  DWORD Args[1];

  Args[0] = (DWORD) idComDev;

  return ((*pfnUTProc) (Args, CLEARCOMMBREAK, NULL));
}
